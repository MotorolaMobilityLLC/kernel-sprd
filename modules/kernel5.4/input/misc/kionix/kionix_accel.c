/* drivers/input/misc/kionix_accel.c - Kionix accelerometer driver
 *
 * Copyright (C) 2012 Kionix, Inc.
 * Written by Kuching Tan <kuchingtan@kionix.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.	 If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <kionix_accel.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_HAS_EARLYSUSPEND */
#include	<linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
/* Debug Message Flags */
#define KIONIX_KMSG_ERR	1	/* Print kernel debug message for error */
#define KIONIX_KMSG_INF	1	/* Print kernel debug message for info */
#define USE_WAIT_QUEUE 1
#if USE_WAIT_QUEUE
static struct task_struct *thread;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int gsensor_flag;
#endif
#if KIONIX_KMSG_ERR
#define KMSGERR(format, ...)	\
dev_err(format, ## __VA_ARGS__)
#else
#define KMSGERR(format, ...)
#endif
#if KIONIX_KMSG_INF
#define KMSGINF(format, ...)	\
dev_info(format, ## __VA_ARGS__)
#else
#define KMSGINF(format, ...)
#endif
/******************************************************************************
* Accelerometer WHO_AM_I return value
*****************************************************************************/
#define KIONIX_ACCEL_WHO_AM_I_KXTE9			0x00
#define KIONIX_ACCEL_WHO_AM_I_KXTF9			0x01
#define KIONIX_ACCEL_WHO_AM_I_KXTI9_1001	0x04
#define KIONIX_ACCEL_WHO_AM_I_KXTIK_1004	0x05
#define KIONIX_ACCEL_WHO_AM_I_KXTJ9_1005	0x07
#define KIONIX_ACCEL_WHO_AM_I_KXTJ9_1007	0x08
#define KIONIX_ACCEL_WHO_AM_I_KXCJ9_1008	0x0A
#define KIONIX_ACCEL_WHO_AM_I_KXTJ2_1009	0x09
#define KIONIX_ACCEL_WHO_AM_I_KXTJ3_105X	0x35
#define KIONIX_ACCEL_WHO_AM_I_KXCJK_1013	0x11
#define KIONIX_ACCEL_WHO_AM_I_KX023		0x15
/******************************************************************************
* Accelerometer Grouping
*****************************************************************************/
#define KIONIX_ACCEL_GRP1	1	/* KXTE9 */
#define KIONIX_ACCEL_GRP2	2	/* KXTF9/I9-1001/J9-1005 */
#define KIONIX_ACCEL_GRP3	3	/* KXTIK-1004 */
#define KIONIX_ACCEL_GRP4	4	/* KXTJ9-1007/KXCJ9-1008 */
#define KIONIX_ACCEL_GRP5	5	/* KXTJ2-1009 */
#define KIONIX_ACCEL_GRP6	6	/* KXCJK-1013 */
#define KIONIX_ACCEL_GRP7	7	/* KX023 */
static char chip_info[20];
/******************************************************************************
* Registers for All Accelerometer Group
*****************************************************************************/
#define ACCEL_WHO_AM_I		0x0F
/*****************************************************************************/
/* Registers for Accelerometer Group 1 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP1_XOUT			0x12
/* Control Registers */
#define ACCEL_GRP1_CTRL_REG1	0x1B
/* CTRL_REG1 */
#define ACCEL_GRP1_PC1_OFF		0x7F
#define ACCEL_GRP1_PC1_ON		(1 << 7)
#define ACCEL_GRP1_ODR40		(3 << 3)
#define ACCEL_GRP1_ODR10		(2 << 3)
#define ACCEL_GRP1_ODR3			(1 << 3)
#define ACCEL_GRP1_ODR1			(0 << 3)
#define ACCEL_GRP1_ODR_MASK		(3 << 3)
/*****************************************************************************/
/* Registers for Accelerometer Group 2 & 3 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP2_XOUT_L		0x06
/* Control Registers */
#define ACCEL_GRP2_INT_REL		0x1A
#define ACCEL_GRP2_CTRL_REG1	0x1B
#define ACCEL_GRP2_INT_CTRL1	0x1E
#define ACCEL_GRP2_DATA_CTRL	0x21
/* CTRL_REG1 */
#define ACCEL_GRP2_PC1_OFF		0x7F
#define ACCEL_GRP2_PC1_ON		(1 << 7)
#define ACCEL_GRP2_DRDYE		(1 << 5)
#define ACCEL_GRP2_G_8G			(2 << 3)
#define ACCEL_GRP2_G_4G			(1 << 3)
#define ACCEL_GRP2_G_2G			(0 << 3)
#define ACCEL_GRP2_G_MASK		(3 << 3)
#define ACCEL_GRP2_RES_8BIT		(0 << 6)
#define ACCEL_GRP2_RES_12BIT	(1 << 6)
#define ACCEL_GRP2_RES_MASK		(1 << 6)
/* INT_CTRL1 */
#define ACCEL_GRP2_IEA			(1 << 4)
#define ACCEL_GRP2_IEN			(1 << 5)
/* DATA_CTRL_REG */
#define ACCEL_GRP2_ODR12_5		0x00
#define ACCEL_GRP2_ODR25		0x01
#define ACCEL_GRP2_ODR50		0x02
#define ACCEL_GRP2_ODR100		0x03
#define ACCEL_GRP2_ODR200		0x04
#define ACCEL_GRP2_ODR400		0x05
#define ACCEL_GRP2_ODR800		0x06
/*****************************************************************************/
/*****************************************************************************/
/* Registers for Accelerometer Group 4 & 5 & 6 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP4_XOUT_L		0x06
/* Control Registers */
#define ACCEL_GRP4_INT_REL		0x1A
#define ACCEL_GRP4_CTRL_REG1	0x1B
#define ACCEL_GRP4_INT_CTRL1	0x1E
#define ACCEL_GRP4_DATA_CTRL	0x21
/* CTRL_REG1 */
#define ACCEL_GRP4_PC1_OFF		0x7F
#define ACCEL_GRP4_PC1_ON		(1 << 7)
#define ACCEL_GRP4_DRDYE		(1 << 5)
#define ACCEL_GRP4_G_8G			(2 << 3)
#define ACCEL_GRP4_G_4G			(1 << 3)
#define ACCEL_GRP4_G_2G			(0 << 3)
#define ACCEL_GRP4_G_MASK		(3 << 3)
#define ACCEL_GRP4_RES_8BIT		(0 << 6)
#define ACCEL_GRP4_RES_12BIT	(1 << 6)
#define ACCEL_GRP4_RES_MASK		(1 << 6)
/* INT_CTRL1 */
#define ACCEL_GRP4_IEA			(1 << 4)
#define ACCEL_GRP4_IEN			(1 << 5)
/* DATA_CTRL_REG */
#define ACCEL_GRP4_ODR0_781		0x08
#define ACCEL_GRP4_ODR1_563		0x09
#define ACCEL_GRP4_ODR3_125		0x0A
#define ACCEL_GRP4_ODR6_25		0x0B
#define ACCEL_GRP4_ODR12_5		0x00
#define ACCEL_GRP4_ODR25		0x01
#define ACCEL_GRP4_ODR50		0x02
#define ACCEL_GRP4_ODR100		0x03
#define ACCEL_GRP4_ODR200		0x04
#define ACCEL_GRP4_ODR400		0x05
#define ACCEL_GRP4_ODR800		0x06
#define ACCEL_GRP4_ODR1600		0x07
/*****************************************************************************/
/*****************************************************************************/
/* Registers for Accelerometer Group 7 */
/*****************************************************************************/
/* Output Registers */
#define ACCEL_GRP7_XOUT_L		0x06
/* Control Registers */
#define ACCEL_GRP7_INT_REL		0x17
#define ACCEL_GRP7_CTRL_REG1	0x18
#define ACCEL_GRP7_INT_CTRL1	0x1C
#define ACCEL_GRP7_DATA_CTRL	0x1B	/* ODCNTL */
/* CTRL_REG1 */
#define ACCEL_GRP7_PC1_OFF		0x7F
#define ACCEL_GRP7_PC1_ON		(1 << 7)
#define ACCEL_GRP7_DRDYE		(1 << 5)
#define ACCEL_GRP7_G_8G			(2 << 3)
#define ACCEL_GRP7_G_4G			(1 << 3)
#define ACCEL_GRP7_G_2G			(0 << 3)
#define ACCEL_GRP7_G_MASK		(3 << 3)
#define ACCEL_GRP7_RES_8BIT		(0 << 6)
#define ACCEL_GRP7_RES_16BIT	(1 << 6)
#define ACCEL_GRP7_RES_MASK		(1 << 6)
/* INT_CTRL1 */
#define ACCEL_GRP7_IEA			(1 << 4)
#define ACCEL_GRP7_IEN			(1 << 5)
/* DATA_CTRL_REG */
#define ACCEL_GRP7_ODR0_781		0x08
#define ACCEL_GRP7_ODR1_563		0x09
#define ACCEL_GRP7_ODR3_125		0x0A
#define ACCEL_GRP7_ODR6_25		0x0B
#define ACCEL_GRP7_ODR12_5		0x00
#define ACCEL_GRP7_ODR25		0x01
#define ACCEL_GRP7_ODR50		0x02
#define ACCEL_GRP7_ODR100		0x03
#define ACCEL_GRP7_ODR200		0x04
#define ACCEL_GRP7_ODR400		0x05
#define ACCEL_GRP7_ODR800		0x06
#define ACCEL_GRP7_ODR1600		0x07
/*****************************************************************************/
/* Input Event Constants */
#define ACCEL_G_MAX			8096
#define ACCEL_FUZZ			0
#define ACCEL_FLAT			0
/* I2C Retry Constants */
#define KIONIX_I2C_RETRY_COUNT		10	/* Number of times to retry i2c */
#define KIONIX_I2C_RETRY_TIMEOUT	1	/* Timeout between retry (miliseconds) */
/* Earlysuspend Contants */
#define KIONIX_ACCEL_EARLYSUSPEND_TIMEOUT	5000	/* Timeout (miliseconds) */
/*
* The following table lists the maximum appropriate poll interval for each
* available output data rate (ODR).
*/
static const struct {
	unsigned int cutoff;
	u8 mask;
} kionix_accel_grp1_odr_table[] = {
	{ 100,	ACCEL_GRP1_ODR40 },
	{ 334,	ACCEL_GRP1_ODR10 },
	{ 1000,	ACCEL_GRP1_ODR3	 },
	{ 0,	ACCEL_GRP1_ODR1	 },
};

static const struct {
	unsigned int cutoff;
	u8 mask;
} kionix_accel_grp2_odr_table[] = {
	{ 3,	ACCEL_GRP2_ODR800 },
	{ 5,	ACCEL_GRP2_ODR400 },
	{ 10,	ACCEL_GRP2_ODR200 },
	{ 20,	ACCEL_GRP2_ODR100 },
	{ 40,	ACCEL_GRP2_ODR50  },
	{ 80,	ACCEL_GRP2_ODR25  },
	{ 0,	ACCEL_GRP2_ODR12_5},
};

static const struct {
	unsigned int cutoff;
	u8 mask;
} kionix_accel_grp4_odr_table[] = {
	{ 2,	ACCEL_GRP4_ODR1600 },
	{ 3,	ACCEL_GRP4_ODR800 },
	{ 5,	ACCEL_GRP4_ODR400 },
	{ 10,	ACCEL_GRP4_ODR200 },/*ACCEL_GRP4_ODR200*/
	{ 20,	ACCEL_GRP4_ODR100 },
	{ 40,	ACCEL_GRP4_ODR50  },
	{ 80,	ACCEL_GRP4_ODR25  },
	{ 160,	ACCEL_GRP4_ODR12_5},
	{ 320,	ACCEL_GRP4_ODR6_25},
	{ 640,	ACCEL_GRP4_ODR3_125},
	{ 1280,	ACCEL_GRP4_ODR1_563},
	{ 0,	ACCEL_GRP4_ODR0_781},
};

static const struct {
	unsigned int cutoff;
	u8 mask;
} kionix_accel_grp7_odr_table[] = {
	{ 2,	ACCEL_GRP7_ODR1600 },
	{ 3,	ACCEL_GRP7_ODR800 },
	{ 5,	ACCEL_GRP7_ODR400 },
	{ 10,	ACCEL_GRP7_ODR200 },
	{ 20,	ACCEL_GRP7_ODR100 },
	{ 40,	ACCEL_GRP7_ODR50  },
	{ 80,	ACCEL_GRP7_ODR25  },
	{ 160,	ACCEL_GRP7_ODR12_5},
	{ 320,	ACCEL_GRP7_ODR6_25},
	{ 640,	ACCEL_GRP7_ODR3_125},
	{ 1280,	ACCEL_GRP7_ODR1_563},
	{ 0,	ACCEL_GRP7_ODR0_781},
};

enum {
	accel_grp1_ctrl_reg1 = 0,
	accel_grp1_regs_count,
};
enum {
	accel_grp2_ctrl_reg1 = 0,
	accel_grp2_data_ctrl,
	accel_grp2_int_ctrl,
	accel_grp2_regs_count,
};
enum {
	accel_grp4_ctrl_reg1 = 0,
	accel_grp4_data_ctrl,
	accel_grp4_int_ctrl,
	accel_grp4_regs_count,
};
enum {
	accel_grp7_ctrl_reg1 = 0,
	accel_grp7_data_ctrl,
	accel_grp7_int_ctrl,
	accel_grp7_regs_count,
};
#define ORI 0
enum sensors_placement_t {
	OBVERSE_X_AXIS_FORWARD,	/*!< Sensor on the same side of screen, x-axis point forward */
	OBVERSE_X_AXIS_RIGHTWARD,	/*!< Sensor on the same side of screen, x-axis point rightward */
	OBVERSE_X_AXIS_BACKWARD,	/*!< Sensor on the same side of screen, x-axis point backward */
	OBVERSE_X_AXIS_LEFTWARD,	/*!< Sensor on the same side of screen, x-axis point leftward */
	REVERSE_X_AXIS_FORWARD,	/*!< Sensor on the reverse side of screen, x-axis point forward */
	REVERSE_X_AXIS_RIGHTWARD,	/*!< Sensor on the reverse side of screen, x-axis point rightward */
	REVERSE_X_AXIS_BACKWARD,	/*!< Sensor on the reverse side of screen, x-axis point backward */
	REVERSE_X_AXIS_LEFTWARD	/*!< Sensor on the reverse side of screen, x-axis point leftward */
};
static void coordinate_data_convert(int *vec_io, int dir)
{
	int tmp;

	if (!vec_io)
		return;

	switch (dir) {
	case OBVERSE_X_AXIS_FORWARD:
		/*x'=-y y'=x z'=z */
		tmp = vec_io[0];
		vec_io[0] = -vec_io[1];
		vec_io[1] = tmp;
		break;

	case OBVERSE_X_AXIS_RIGHTWARD:
		break;

	case OBVERSE_X_AXIS_BACKWARD:
		/*x'=y y'=-x z'=z */
		tmp = vec_io[0];
		vec_io[0] = vec_io[1];
		vec_io[1] = -tmp;
		break;

	case OBVERSE_X_AXIS_LEFTWARD:
		/*x'=-x y'=-y z'=z */
		vec_io[0] = -vec_io[0];
		vec_io[1] = -vec_io[1];
		break;

	case REVERSE_X_AXIS_FORWARD:
		/*x'=y y'=x z'=-z */
		tmp = vec_io[0];
		vec_io[0] = vec_io[1];
		vec_io[1] = tmp;
		vec_io[2] = -vec_io[2];
		break;

	case REVERSE_X_AXIS_RIGHTWARD:
		/*x'=x y'=-y z'=-z */
		vec_io[0] = vec_io[0];
		vec_io[1] = -vec_io[1];
		vec_io[2] = -vec_io[2];
		break;

	case REVERSE_X_AXIS_BACKWARD:
		/*x'=-y y'=-x z'=-z */
		tmp = vec_io[0];
		vec_io[0] = -vec_io[1];
		vec_io[1] = -tmp;
		vec_io[2] = -vec_io[2];
		break;

	case REVERSE_X_AXIS_LEFTWARD:
		/*x'=-x y'=y z'=-z */
		vec_io[0] = -vec_io[0];
		vec_io[1] = vec_io[1];
		vec_io[2] = -vec_io[2];
		break;

	default:
		break;
	}
}

/*one-out-three SENSOR_2G SENSOR_4G SENSOR_8G*/
#define SENSOR_2G 1
/*#define SENSOR_4G 1*/
/*#define SENSOR_8G 1*/
#define G_SENSOR_ZERO 0
#ifdef SENSOR_2G
#define G_SENSOR_ONE_G 1024
#endif
#ifdef SENSOR_4G
#define G_SENSOR_ONE_G 0
#endif
#ifdef SENSOR_8G
#define G_SENSOR_ONE_G 0
#endif
#define G_SENSOR_X 0
#define G_SENSOR_Y 1
#define G_SENSOR_Z 2
#define G_SENSOR_ALL 3
#define G_SENSOR_CAIL_NUM 30
#define TAKE_OUT 3
#define G_SENSOR_CALI_PROCNAME "g_sensor_cali"
int g_chip_id;
struct kionix_accel_driver {
	struct i2c_client *client;
	struct kionix_accel_platform_data accel_pdata;
	struct input_dev *input_dev;
	struct delayed_work accel_work;
	struct workqueue_struct *accel_workqueue;
	int accel_data[3];
	int accel_cali[3];
	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;
	bool negate_x;
	bool negate_y;
	bool negate_z;
	u8 shift;
	unsigned int poll_interval;
	unsigned int poll_delay;
	unsigned int accel_group;
	u8 *accel_registers;
	atomic_t accel_enabled;
	unsigned int on_before_suspend;
	struct mutex mutex_enable;
	rwlock_t rwlock_accel_data;
	bool accel_drdy;
	/* Function callback */
	void (*kionix_accel_report_accel_data)(struct kionix_accel_driver *
										   acceld);
	int (*kionix_accel_update_odr)(struct kionix_accel_driver *acceld,
								   unsigned int poll_interval);
	int (*kionix_accel_power_on_init)(struct kionix_accel_driver *acceld);
	int (*kionix_accel_operate)(struct kionix_accel_driver *acceld);
	int (*kionix_accel_standby)(struct kionix_accel_driver *acceld);
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif				/* CONFIG_HAS_EARLYSUSPEND */
};
#ifdef CONFIG_GET_DEVICES_VENDOR_INFO
char g_sensor_vendor_info[20];
char g_sensor_ic_info[20];
#endif
static struct proc_dir_entry *g_sensor_proc_entry;
static int g_sensor_array[G_SENSOR_ALL][G_SENSOR_CAIL_NUM];
static int g_sensor_compensation[G_SENSOR_ALL];
static int g_sensor_sum[G_SENSOR_ALL];
static int point_num = 11;
struct kionix_accel_driver *g_acceld;
static int kionix_accel_enable(struct kionix_accel_driver *acceld);
static int kionix_accel_disable(struct kionix_accel_driver *acceld);
static void init_g_sensor_array(void)
{
	memset(g_sensor_sum, 0, sizeof(int) * G_SENSOR_ALL);
	/**/ memset(g_sensor_compensation, 0, sizeof(int) * G_SENSOR_ALL);
}

static void Bubble_2(int *r, int n)
{
	int low = 0;
	int high = n - 1;
	int tmp, j;

	while (low < high) {
		for (j = low; j < high; ++j)
			if (r[j] > r[j + 1]) {
				tmp = r[j];
				r[j] = r[j + 1];
				r[j + 1] = tmp;
			}

		--high;

		for (j = high; j > low; --j)
			if (r[j] < r[j - 1]) {
				tmp = r[j];
				r[j] = r[j - 1];
				r[j - 1] = tmp;
			}

		++low;
	}
}

/*struct file *, const char __user *, size_t, loff_t **/
static ssize_t g_sensor_cali_write(struct file *filp, const char __user *buff,
							   size_t len, loff_t *data)
{
	int i = 0;
	char buffer[12];
	int *buffer_int = NULL;

	printk(KERN_INFO " wellcome to g_sensor_cali_write !!");

	if (len != 12) {
		printk("g_sensor_cali_write need 12 char !!");
		return -1;
	}

	memset(buffer, 0, 12);

	if (copy_from_user((void *)&buffer[0], (const void __user *)buff, len))
		return -EFAULT;

	buffer_int = (int *)buffer;
	memset(g_sensor_compensation, 0, sizeof(int) * G_SENSOR_ALL);
	g_sensor_compensation[0] = buffer_int[0];
	g_sensor_compensation[1] = buffer_int[1];
	g_sensor_compensation[2] = buffer_int[2];

	if (g_acceld == NULL) {
		printk("g_acceld NULL !!");
		return -EFAULT;
	}

	g_acceld->accel_cali[g_acceld->axis_map_x] = g_sensor_compensation[0];
	g_acceld->accel_cali[g_acceld->axis_map_y] = g_sensor_compensation[1];
	g_acceld->accel_cali[g_acceld->axis_map_z] = g_sensor_compensation[2];

	for (i = 0; i < G_SENSOR_ALL; i++) {
		printk("g_sensor_cali_write [%d]:%d ", i,
			   g_sensor_compensation[i]);
	}

	return len;
}

/*struct file *, char __user *, size_t, loff_t **/
static ssize_t g_sensor_cali_read(struct file *file, char __user *buf, size_t size,
							  loff_t *ppos)
{
	int i = 0;
	int j = 0;

	printk(KERN_INFO " wellcome to g_sensor_cali_read !!\n");

	if (g_acceld == NULL) {
		printk(KERN_ERR "g_acceld is NULL !!\n");
		return -1;
	}

	memset(g_sensor_array, 0,
		   sizeof(int) * G_SENSOR_ALL * G_SENSOR_CAIL_NUM);

	if (kionix_accel_enable(g_acceld) != 0) {
		printk(KERN_ERR "can't enable sensor data!!\n");
		return -1;
	}

	atomic_set(&g_acceld->accel_enabled, 1);
	point_num = 0;
	msleep(600);

	if (point_num == 0) {
		printk(KERN_ERR "no sensor data!!\n");
		return -1;
	}
	printk(KERN_INFO "sensor data ok!!\n");

	if (kionix_accel_disable(g_acceld) != 0) {
		printk(KERN_ERR "can't disable sensor data!!\n");
		return -1;
	}

	atomic_set(&g_acceld->accel_enabled, 0);
	init_g_sensor_array();

	/* 2 save data */
	/* 3 sort data */
	for (i = 0; i < G_SENSOR_ALL; i++) {
		Bubble_2(&(g_sensor_array[i][0]), G_SENSOR_CAIL_NUM);
		/*
		  * for(j=0;j<G_SENSOR_CAIL_NUM;j++){
		  * printk("g_sensor_array[%d]:[%d]\n",i,g_sensor_array[i][j]);
		  * }
		  */
	}

	/* 4 cail data */
	for (i = 0; i < G_SENSOR_ALL; i++) {
		for (j = TAKE_OUT; j < (G_SENSOR_CAIL_NUM - TAKE_OUT); j++)
			g_sensor_sum[i] += g_sensor_array[i][j];

		/*printk("g_sensor_sum[%d]:[%d]\n",i,g_sensor_sum[i]); */
		g_sensor_compensation[i] =
			g_sensor_sum[i] / (G_SENSOR_CAIL_NUM - (TAKE_OUT * 2));
	}

	/* 5 return data */
	g_sensor_compensation[0] -= G_SENSOR_ZERO;
	g_sensor_compensation[1] -= G_SENSOR_ZERO;
	g_sensor_compensation[2] -= G_SENSOR_ONE_G;
	g_acceld->accel_cali[g_acceld->axis_map_x] = g_sensor_compensation[0];
	g_acceld->accel_cali[g_acceld->axis_map_y] = g_sensor_compensation[1];
	g_acceld->accel_cali[g_acceld->axis_map_z] = g_sensor_compensation[2];

	for (i = 0; i < G_SENSOR_ALL; i++) {
		printk(KERN_INFO "end the cali g_sensor_compensation[%d]:[%d]\n", i,
			   g_sensor_compensation[i]);
	}

	if (copy_to_user(buf, (char *)g_sensor_compensation, 12))
		return -EFAULT;


	return 12;
}

static const struct file_operations g_sensor_cali_ops = {
	.owner = THIS_MODULE,
	.read = g_sensor_cali_read,
	.write = g_sensor_cali_write,
};

static int kionix_i2c_read(struct i2c_client *client, u8 addr, u8 *data,
						   int len)
{
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 1,
			.buf = &addr,
		},
		{
			.addr = client->addr,
			.flags = client->flags | I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};
	return i2c_transfer(client->adapter, msgs, 2);
}

static int kionix_strtok(const char *buf, size_t count, char **token,
						 const int token_nr)
{
	char *buf2 = kzalloc((count + 1) * sizeof(char), GFP_KERNEL);
	char **token2 = token;
	unsigned int num_ptr = 0, num_nr = 0, num_neg = 0;
	int i = 0, start = 0, end = (int)count;

	strcpy(buf2, buf);

	/* We need to breakup the string into separate chunks in order for kstrtoint
	 * or strict_strtol to parse them without returning an error. Stop when the end of
	 * the string is reached or when enough value is read from the string
	 */
	while ((start < end) && (i < token_nr)) {
		/* We found a negative sign */
		if (*(buf2 + start) == '-') {
			/* Previous char(s) are numeric, so we store their value first before proceed */
			if (num_nr > 0) {
				/* If there is a pending negative sign, we adjust the variables to account for it */
				if (num_neg) {
					num_ptr--;
					num_nr++;
				}

				*token2 =
					kzalloc((num_nr + 2) * sizeof(char),
									GFP_KERNEL);
				strncpy(*token2, (const char *)(buf2 + num_ptr),
						(size_t) num_nr);
				*(*token2 + num_nr) = '\n';
				i++;
				token2++;
				/* Reset */
				num_ptr = num_nr = 0;
			}

			/* This indicates that there is a pending negative sign in the string */
			num_neg = 1;
		}
		/* We found a numeric */
		else if ((*(buf2 + start) >= '0') && (*(buf2 + start) <= '9')) {
			/* If the previous char(s) are not numeric, set num_ptr to current char */
			if (num_nr < 1)
				num_ptr = start;

			num_nr++;
		}
		/* We found an unwanted character */
		else {
			/* Previous char(s) are numeric, so we store their value first before proceed */
			if (num_nr > 0) {
				if (num_neg) {
					num_ptr--;
					num_nr++;
				}

				*token2 =
					kzalloc((num_nr + 2) * sizeof(char), GFP_KERNEL);
				strncpy(*token2, (const char *)(buf2 + num_ptr),
						(size_t) num_nr);
				*(*token2 + num_nr) = '\n';
				i++;
				token2++;
			}

			/* Reset all the variables to start afresh */
			num_ptr = num_nr = num_neg = 0;
		}

		start++;
	}

	kfree(buf2);
	return (i == token_nr) ? token_nr : -1;
}

static int kionix_accel_grp1_power_on_init(struct kionix_accel_driver *acceld)
{
	int err;

	if (atomic_read(&acceld->accel_enabled) > 0) {
		err = i2c_smbus_write_byte_data(acceld->client,
										ACCEL_GRP1_CTRL_REG1,
										acceld->
										accel_registers
										[accel_grp1_ctrl_reg1] |
										ACCEL_GRP1_PC1_ON);

		if (err < 0)
			return err;
	} else {
		err = i2c_smbus_write_byte_data(acceld->client,
										ACCEL_GRP1_CTRL_REG1,
										acceld->
										accel_registers
										[accel_grp1_ctrl_reg1]);

		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp1_operate(struct kionix_accel_driver *acceld)
{
	int err;

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP1_CTRL_REG1,
									acceld->
									accel_registers[accel_grp2_ctrl_reg1] |
									ACCEL_GRP1_PC1_ON);

	if (err < 0)
		return err;

	queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work, 0);
	return 0;
}

static int kionix_accel_grp1_standby(struct kionix_accel_driver *acceld)
{
	int err;
	cancel_delayed_work_sync(&acceld->accel_work);
	err =
		i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP1_CTRL_REG1, 0);

	if (err < 0)
		return err;

	return 0;
}

static void kionix_accel_grp1_report_accel_data(struct kionix_accel_driver
		*acceld)
{
	u8 accel_data[3];
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = acceld->input_dev;
	int loop = KIONIX_I2C_RETRY_COUNT;

	KMSGINF(&acceld->client->dev, "<%s> start L%d.\n", __func__, __LINE__);

	if (atomic_read(&acceld->accel_enabled) > 0) {
		{
			while (loop) {
				mutex_lock(&input_dev->mutex);
				err =
					kionix_i2c_read(acceld->client,
									ACCEL_GRP1_XOUT, accel_data,
									6);
				mutex_unlock(&input_dev->mutex);

				if (err < 0) {
					loop--;
					mdelay(KIONIX_I2C_RETRY_TIMEOUT);
				} else
					loop = 0;
			}

			if (err < 0) {
				KMSGERR(&acceld->client->dev,
						"%s: read data output error = %d\n",
						__func__, err);
			} else {
				write_lock(&acceld->rwlock_accel_data);
				x = ((s16)
					 le16_to_cpu(((s16)
								  (accel_data
								   [acceld->axis_map_x] >> 2)) -
								 32)) << 6;
				y = ((s16)
					 le16_to_cpu(((s16)
								  (accel_data
								   [acceld->axis_map_y] >> 2)) -
								 32)) << 6;
				z = ((s16)
					 le16_to_cpu(((s16)
								  (accel_data
								   [acceld->axis_map_z] >> 2)) -
								 32)) << 6;
				acceld->accel_data[acceld->axis_map_x] =
					(acceld->negate_x ? -x : x) +
					acceld->accel_cali[acceld->axis_map_x];
				acceld->accel_data[acceld->axis_map_y] =
					(acceld->negate_y ? -y : y) +
					acceld->accel_cali[acceld->axis_map_y];
				acceld->accel_data[acceld->axis_map_z] =
					(acceld->negate_z ? -z : z) +
					acceld->accel_cali[acceld->axis_map_z];
				{
					input_report_abs(acceld->input_dev,
									 ABS_X,
									 acceld->
									 accel_data[acceld->
												axis_map_x]);
					input_report_abs(acceld->input_dev,
									 ABS_Y,
									 acceld->
									 accel_data[acceld->
												axis_map_y]);
					input_report_abs(acceld->input_dev,
									 ABS_Z,
									 acceld->
									 accel_data[acceld->
												axis_map_z]);
					input_sync(acceld->input_dev);
				}
				write_unlock(&acceld->rwlock_accel_data);
			}
		}
	}

	KMSGINF(&acceld->client->dev, "<%s> x[%d], y[%d], z[%d], L%d.\n",
			__func__, acceld->accel_data[acceld->axis_map_x],
			acceld->accel_data[acceld->axis_map_y],
			acceld->accel_data[acceld->axis_map_z], __LINE__);
}

static int kionix_accel_grp1_update_odr(struct kionix_accel_driver *acceld,
										unsigned int poll_interval)
{
	int err;
	int i;
	u8 odr;

	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kionix_accel_grp1_odr_table); i++) {
		odr = kionix_accel_grp1_odr_table[i].mask;

		if (poll_interval < kionix_accel_grp1_odr_table[i].cutoff)
			break;
	}

	/* Do not need to update CTRL_REG1 register if the ODR is not changed */
	if ((acceld->
		 accel_registers[accel_grp1_ctrl_reg1] & ACCEL_GRP1_ODR_MASK) ==
		odr)
		return 0;

	acceld->accel_registers[accel_grp1_ctrl_reg1] &= ~ACCEL_GRP1_ODR_MASK;
	acceld->accel_registers[accel_grp1_ctrl_reg1] |= odr;

	/* Do not need to update CTRL_REG1 register if the sensor is not currently turn on */
	if (atomic_read(&acceld->accel_enabled) > 0) {
		err =
			i2c_smbus_write_byte_data(acceld->client,
									  ACCEL_GRP1_CTRL_REG1,
									  acceld->
									  accel_registers
									  [accel_grp1_ctrl_reg1] |
									  ACCEL_GRP1_PC1_ON);

		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp2_power_on_init(struct kionix_accel_driver *acceld)
{
	int err;
	/* ensure that PC1 is cleared before updating control registers */
	err = i2c_smbus_write_byte_data(acceld->client,
									ACCEL_GRP2_CTRL_REG1, 0);

	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(acceld->client,
									ACCEL_GRP2_DATA_CTRL,
									acceld->
									accel_registers[accel_grp2_data_ctrl]);

	if (err < 0)
		return err;

	/* only write INT_CTRL_REG1 if in irq mode */
	if (acceld->client->irq) {
		err = i2c_smbus_write_byte_data(acceld->client,
										ACCEL_GRP2_INT_CTRL1,
										acceld->
										accel_registers
										[accel_grp2_int_ctrl]);

		if (err < 0)
			return err;
	}

	if (atomic_read(&acceld->accel_enabled) > 0) {
		err = i2c_smbus_write_byte_data(acceld->client,
										ACCEL_GRP2_CTRL_REG1,
										acceld->
										accel_registers
										[accel_grp2_ctrl_reg1] |
										ACCEL_GRP2_PC1_ON);

		if (err < 0)
			return err;
	} else {
		err = i2c_smbus_write_byte_data(acceld->client,
										ACCEL_GRP2_CTRL_REG1,
										acceld->
										accel_registers
										[accel_grp2_ctrl_reg1]);

		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp2_operate(struct kionix_accel_driver *acceld)
{
	int err;

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP2_CTRL_REG1,
									acceld->
									accel_registers[accel_grp2_ctrl_reg1] |
									ACCEL_GRP2_PC1_ON);

	if (err < 0)
		return err;

	if (acceld->accel_drdy == 0)
		queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work,
						   0);

	return 0;
}

static int kionix_accel_grp2_standby(struct kionix_accel_driver *acceld)
{
	int err;

	if (acceld->accel_drdy == 0)
		cancel_delayed_work_sync(&acceld->accel_work);

	err =
		i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP2_CTRL_REG1, 0);

	if (err < 0)
		return err;

	return 0;
}

static void kionix_accel_grp2_report_accel_data(struct kionix_accel_driver
		*acceld)
{
	struct {
		union {
			s16 accel_data_s16[3];
			s8 accel_data_s8[6];
		};
	} accel_data;
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = acceld->input_dev;
	int loop;

	/* Only read the output registers if enabled */
	if (atomic_read(&acceld->accel_enabled) > 0) {
		{
			loop = KIONIX_I2C_RETRY_COUNT;

			while (loop) {
				mutex_lock(&input_dev->mutex);
				err =
					kionix_i2c_read(acceld->client,
									ACCEL_GRP2_XOUT_L,
									(u8 *) accel_data.
									accel_data_s16, 6);
				mutex_unlock(&input_dev->mutex);

				if (err < 0) {
					loop--;
					mdelay(KIONIX_I2C_RETRY_TIMEOUT);
				} else
					loop = 0;
			}

			if (err < 0) {
				KMSGERR(&acceld->client->dev,
						"%s: read data output error = %d\n",
						__func__, err);
			} else {
				write_lock(&acceld->rwlock_accel_data);
				x = ((s16)
					 le16_to_cpu(accel_data.
								 accel_data_s16[acceld->
												axis_map_x])) >>
					acceld->shift;
				y = ((s16)
					 le16_to_cpu(accel_data.
								 accel_data_s16[acceld->
												axis_map_y])) >>
					acceld->shift;
				z = ((s16)
					 le16_to_cpu(accel_data.
								 accel_data_s16[acceld->
												axis_map_z])) >>
					acceld->shift;
				acceld->accel_data[acceld->axis_map_x] =
					(acceld->negate_x ? -x : x) +
					acceld->accel_cali[acceld->axis_map_x];
				acceld->accel_data[acceld->axis_map_y] =
					(acceld->negate_y ? -y : y) +
					acceld->accel_cali[acceld->axis_map_y];
				acceld->accel_data[acceld->axis_map_z] =
					(acceld->negate_z ? -z : z) +
					acceld->accel_cali[acceld->axis_map_z];
				{
					input_report_abs(acceld->input_dev,
									 ABS_X,
									 acceld->
									 accel_data[acceld->
												axis_map_x]);
					input_report_abs(acceld->input_dev,
									 ABS_Y,
									 acceld->
									 accel_data[acceld->
												axis_map_y]);
					input_report_abs(acceld->input_dev,
									 ABS_Z,
									 acceld->
									 accel_data[acceld->
												axis_map_z]);
					input_sync(acceld->input_dev);
				}
				write_unlock(&acceld->rwlock_accel_data);
			}
		}
	}

	/* Clear the interrupt if using drdy */
	if (acceld->accel_drdy == 1) {
		loop = KIONIX_I2C_RETRY_COUNT;

		while (loop) {
			err =
				i2c_smbus_read_byte_data(acceld->client,
										 ACCEL_GRP2_INT_REL);

			if (err < 0) {
				loop--;
				mdelay(KIONIX_I2C_RETRY_TIMEOUT);
			} else
				loop = 0;
		}

		if (err < 0)
			KMSGERR(&acceld->client->dev,
					"%s: clear interrupt error = %d\n", __func__,
					err);
	}
}

static void kionix_accel_grp2_update_g_range(struct kionix_accel_driver *acceld)
{
	acceld->accel_registers[accel_grp2_ctrl_reg1] &= ~ACCEL_GRP2_G_MASK;

	switch (acceld->accel_pdata.accel_g_range) {
	case KIONIX_ACCEL_G_8G:
	case KIONIX_ACCEL_G_6G:
		acceld->shift = 2;
		acceld->accel_registers[accel_grp2_ctrl_reg1] |=
			ACCEL_GRP2_G_8G;
		break;

	case KIONIX_ACCEL_G_4G:
		acceld->shift = 3;
		acceld->accel_registers[accel_grp2_ctrl_reg1] |=
			ACCEL_GRP2_G_4G;
		break;

	case KIONIX_ACCEL_G_2G:
	default:
		acceld->shift = 4;
		acceld->accel_registers[accel_grp2_ctrl_reg1] |=
			ACCEL_GRP2_G_2G;
		break;
	}

	return;
}

static int kionix_accel_grp2_update_odr(struct kionix_accel_driver *acceld,
										unsigned int poll_interval)
{
	int err;
	int i;
	u8 odr;

	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kionix_accel_grp2_odr_table); i++) {
		odr = kionix_accel_grp2_odr_table[i].mask;

		if (poll_interval < kionix_accel_grp2_odr_table[i].cutoff)
			break;
	}

	/* Do not need to update DATA_CTRL_REG register if the ODR is not changed */
	if (acceld->accel_registers[accel_grp2_data_ctrl] == odr)
		return 0;

	acceld->accel_registers[accel_grp2_data_ctrl] = odr;

	/* Do not need to update DATA_CTRL_REG register if the sensor is not currently turn on */
	if (atomic_read(&acceld->accel_enabled) > 0) {
		err =
			i2c_smbus_write_byte_data(acceld->client,
									  ACCEL_GRP2_CTRL_REG1, 0);

		if (err < 0)
			return err;

		err =
			i2c_smbus_write_byte_data(acceld->client,
									  ACCEL_GRP2_DATA_CTRL,
									  acceld->
									  accel_registers
									  [accel_grp2_data_ctrl]);

		if (err < 0)
			return err;

		err =
			i2c_smbus_write_byte_data(acceld->client,
									  ACCEL_GRP2_CTRL_REG1,
									  acceld->
									  accel_registers
									  [accel_grp2_ctrl_reg1] |
									  ACCEL_GRP2_PC1_ON);

		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp4_power_on_init(struct kionix_accel_driver *acceld)
{
	int err;
	/* ensure that PC1 is cleared before updating control registers */
	err = i2c_smbus_write_byte_data(acceld->client,
									ACCEL_GRP4_CTRL_REG1, 0);

	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(acceld->client,
									ACCEL_GRP4_DATA_CTRL,
									acceld->
									accel_registers[accel_grp4_data_ctrl]);

	if (err < 0)
		return err;

	/* only write INT_CTRL_REG1 if in irq mode */
	if (acceld->client->irq) {
		err = i2c_smbus_write_byte_data(acceld->client,
										ACCEL_GRP4_INT_CTRL1,
										acceld->
										accel_registers
										[accel_grp4_int_ctrl]);

		if (err < 0)
			return err;
	}

	if (atomic_read(&acceld->accel_enabled) > 0) {
		err = i2c_smbus_write_byte_data(acceld->client,
										ACCEL_GRP4_CTRL_REG1,
										acceld->
										accel_registers
										[accel_grp4_ctrl_reg1] |
										ACCEL_GRP4_PC1_ON);

		if (err < 0)
			return err;
	} else {
		err = i2c_smbus_write_byte_data(acceld->client,
										ACCEL_GRP4_CTRL_REG1,
										acceld->
										accel_registers
										[accel_grp4_ctrl_reg1]);

		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp4_operate(struct kionix_accel_driver *acceld)
{
	int err;

	err =
		i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP4_CTRL_REG1, 0);

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP4_CTRL_REG1,
									acceld->
									accel_registers[accel_grp4_ctrl_reg1]);

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP4_CTRL_REG1,
									acceld->
									accel_registers[accel_grp4_ctrl_reg1] |
									ACCEL_GRP4_PC1_ON);

	if (err < 0)
		return err;

	if (acceld->accel_drdy == 0)
		queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work,
						   0);

	return 0;
}

static int kionix_accel_grp4_standby(struct kionix_accel_driver *acceld)
{
	int err;

	if (acceld->accel_drdy == 0)
		cancel_delayed_work_sync(&acceld->accel_work);

	err =
		i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP4_CTRL_REG1, 0);

	if (err < 0)
		return err;

	return 0;
}

static void kionix_accel_grp4_report_accel_data(struct kionix_accel_driver
		*acceld)
{
	struct {
		union {
			s16 accel_data_s16[3];
			s8 accel_data_s8[6];
		};
	} accel_data;
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = acceld->input_dev;
	int loop;
	int xyz[3] = { 0 };

	/* Only read the output registers if enabled */
	/* KMSGINF(&acceld->client->dev, "%s: report data\n", __func__); */
	if (atomic_read(&acceld->accel_enabled) > 0) {
		{
			loop = KIONIX_I2C_RETRY_COUNT;

			while (loop) {
				mutex_lock(&input_dev->mutex);
				err =
					kionix_i2c_read(acceld->client,
									ACCEL_GRP4_XOUT_L,
									(u8 *) accel_data.
									accel_data_s16, 6);
				mutex_unlock(&input_dev->mutex);

				if (err < 0) {
					loop--;
					mdelay(KIONIX_I2C_RETRY_TIMEOUT);
				} else
					loop = 0;
			}

			if (err < 0) {
				KMSGERR(&acceld->client->dev,
						"%s: read data output error = %d\n",
						__func__, err);
			} else {
				write_lock(&acceld->rwlock_accel_data);
				x = ((s16)
					 le16_to_cpu(accel_data.
								 accel_data_s16[acceld->
												axis_map_x])) >>
					acceld->shift;
				y = ((s16)
					 le16_to_cpu(accel_data.
								 accel_data_s16[acceld->
												axis_map_y])) >>
					acceld->shift;
				z = ((s16)
					 le16_to_cpu(accel_data.
								 accel_data_s16[acceld->
												axis_map_z])) >>
					acceld->shift;

				/*printk("p[%d] x:[%d] y:[%d] z:[%d]\n",point_num,x,y,z); */
				if (point_num < G_SENSOR_CAIL_NUM) {
					g_sensor_array[0][point_num] = x;
					g_sensor_array[1][point_num] = y;
					g_sensor_array[2][point_num] = z;
					point_num++;
				}

				acceld->accel_data[acceld->axis_map_x] =
					(acceld->negate_x ? -x : x) +
					acceld->accel_cali[acceld->axis_map_x];
				acceld->accel_data[acceld->axis_map_y] =
					(acceld->negate_y ? -y : y) +
					acceld->accel_cali[acceld->axis_map_y];
				acceld->accel_data[acceld->axis_map_z] =
					(acceld->negate_z ? -z : z) +
					acceld->accel_cali[acceld->axis_map_z];
				{
					xyz[0] =
						acceld->accel_data[acceld->
										   axis_map_x];
					xyz[1] =
						acceld->accel_data[acceld->
										   axis_map_y];
					xyz[2] =
						acceld->accel_data[acceld->
										   axis_map_z];
					coordinate_data_convert(xyz, ORI);
					/*input_report_abs(acceld->input_dev, ABS_X, acceld->accel_data[acceld->axis_map_x]); */
					/*input_report_abs(acceld->input_dev, ABS_Y, acceld->accel_data[acceld->axis_map_y]); */
					/*input_report_abs(acceld->input_dev, ABS_Z, acceld->accel_data[acceld->axis_map_z]); */
					input_report_abs(acceld->input_dev,
									 ABS_X, xyz[0]);
					/*KMSGINF(&acceld->client->dev, "%s: xyz[0]=%d\n", __func__, xyz[0]); */
					input_report_abs(acceld->input_dev,
									 ABS_Y, xyz[1]);
					/*KMSGINF(&acceld->client->dev, "%s: xyz[1]=%d\n", __func__, xyz[1]); */
					input_report_abs(acceld->input_dev,
									 ABS_Z, xyz[2]);
					/*KMSGINF(&acceld->client->dev, "%s: xyz[2]=%d\n", __func__, xyz[2]); */
					input_sync(acceld->input_dev);
				}
				write_unlock(&acceld->rwlock_accel_data);
			}
		}
	}

	/*		KMSGINF(&acceld->client->dev, "<%s> x[%d], y[%d], z[%d], L%d.\n", */
	/*				__func__, acceld->accel_data[acceld->axis_map_x], */
	/*				acceld->accel_data[acceld->axis_map_y], */
	/*				acceld->accel_data[acceld->axis_map_z],__LINE__); */
	/* Clear the interrupt if using drdy */
	if (acceld->accel_drdy == 1) {
		loop = KIONIX_I2C_RETRY_COUNT;

		while (loop) {
			err =
				i2c_smbus_read_byte_data(acceld->client,
										 ACCEL_GRP4_INT_REL);

			if (err < 0) {
				loop--;
				mdelay(KIONIX_I2C_RETRY_TIMEOUT);
			} else
				loop = 0;
		}

		if (err < 0)
			KMSGERR(&acceld->client->dev,
					"%s: clear interrupt error = %d\n", __func__,
					err);
	}
}

static void kionix_accel_grp4_update_g_range(struct kionix_accel_driver *acceld)
{
	acceld->accel_registers[accel_grp4_ctrl_reg1] &= ~ACCEL_GRP4_G_MASK;

	switch (acceld->accel_pdata.accel_g_range) {
	case KIONIX_ACCEL_G_8G:
	case KIONIX_ACCEL_G_6G:
		acceld->shift = 2;
		acceld->accel_registers[accel_grp4_ctrl_reg1] |=
			ACCEL_GRP4_G_8G;
		break;

	case KIONIX_ACCEL_G_4G:
		acceld->shift = 4;
		acceld->accel_registers[accel_grp4_ctrl_reg1] |=
			ACCEL_GRP4_G_4G;
		break;

	case KIONIX_ACCEL_G_2G:
	default:
		acceld->shift = 4;
		acceld->accel_registers[accel_grp4_ctrl_reg1] |=
			ACCEL_GRP4_G_2G;
		break;
	}

}

static int kionix_accel_grp4_update_odr(struct kionix_accel_driver *acceld,
										unsigned int poll_interval)
{
	int err;
	int i;
	bool enable_statuts;
	u8 odr;

	KMSGINF(&acceld->client->dev, "poll_interval=%u\n", poll_interval);
	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kionix_accel_grp4_odr_table); i++) {
		odr = kionix_accel_grp4_odr_table[i].mask;

		if (poll_interval < kionix_accel_grp4_odr_table[i].cutoff)
			break;
	}
	KMSGINF(&acceld->client->dev, "odr=%u\n", odr);
	/* Do not need to update DATA_CTRL_REG register if the ODR is not changed */
	if (acceld->accel_registers[accel_grp4_data_ctrl] == odr)
		return 0;

	acceld->accel_registers[accel_grp4_data_ctrl] = odr;

	enable_statuts = atomic_read(&acceld->accel_enabled);
	if (enable_statuts == 1) {
		err =
			i2c_smbus_write_byte_data(acceld->client,
									  ACCEL_GRP4_CTRL_REG1, 0);
		if (err < 0) {
			KMSGERR(&acceld->client->dev, "kionix_err=%u\n", err);
			return err;
		}
	}

	err =
		i2c_smbus_write_byte_data(acceld->client,
								  ACCEL_GRP4_DATA_CTRL,
								  acceld->
								  accel_registers
								  [accel_grp4_data_ctrl]);

	if (err < 0) {
		KMSGERR(&acceld->client->dev, "kionix_err=%u\n", err);
		return err;
	}

	if (enable_statuts == 1) {
		err =
			i2c_smbus_write_byte_data(acceld->client,
									  ACCEL_GRP4_CTRL_REG1,
									  acceld->
									  accel_registers
									  [accel_grp4_ctrl_reg1] |
									  ACCEL_GRP4_PC1_ON);
		if (err < 0) {
			KMSGERR(&acceld->client->dev, "kionix_err=%u\n", err);
			return err;
		}
	}

/*#############
	err =
		i2c_smbus_read_byte_data(acceld->client,
								 ACCEL_GRP4_DATA_CTRL);

	if (err < 0) {
		KMSGERR(&acceld->client->dev, "kionix_err=%u\n", err);
		return err;
	}

		switch (err) {
		case ACCEL_GRP4_ODR0_781:
			KMSGINF(&acceld->client->dev, "ODR = 0.781 Hz\n");
			break;

		case ACCEL_GRP4_ODR1_563:
			KMSGINF(&acceld->client->dev, "ODR = 1.563 Hz\n");
			break;

		case ACCEL_GRP4_ODR3_125:
			KMSGINF(&acceld->client->dev, "ODR = 3.125 Hz\n");
			break;

		case ACCEL_GRP4_ODR6_25:
			KMSGINF(&acceld->client->dev, "ODR = 6.25 Hz\n");
			break;

		case ACCEL_GRP4_ODR12_5:
			KMSGINF(&acceld->client->dev, "ODR = 12.5 Hz\n");
			break;

		case ACCEL_GRP4_ODR25:
			KMSGINF(&acceld->client->dev, "ODR = 25 Hz\n");
			break;

		case ACCEL_GRP4_ODR50:
			KMSGINF(&acceld->client->dev, "ODR = 50 Hz\n");
			break;

		case ACCEL_GRP4_ODR100:
			KMSGINF(&acceld->client->dev, "ODR = 100 Hz\n");
			break;

		case ACCEL_GRP4_ODR200:
			KMSGINF(&acceld->client->dev, "ODR = 200 Hz\n");
			break;

		case ACCEL_GRP4_ODR400:
			KMSGINF(&acceld->client->dev, "ODR = 400 Hz\n");
			break;

		case ACCEL_GRP4_ODR800:
			KMSGINF(&acceld->client->dev, "ODR = 800 Hz\n");
			break;

		case ACCEL_GRP4_ODR1600:
			KMSGINF(&acceld->client->dev, "ODR = 1600 Hz\n");
			break;

		default:
			KMSGINF(&acceld->client->dev, "Unknown ODR\n");
			break;
		}
*/
	return 0;
}

static int kionix_accel_grp7_power_on_init(struct kionix_accel_driver *acceld)
{
	int err;
	/* ensure that PC1 is cleared before updating control registers */
	err = i2c_smbus_write_byte_data(acceld->client,
									ACCEL_GRP7_CTRL_REG1, 0);

	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(acceld->client,
									ACCEL_GRP7_DATA_CTRL,
									acceld->
									accel_registers[accel_grp7_data_ctrl]);

	if (err < 0)
		return err;

	/* only write INT_CTRL_REG1 if in irq mode */
	if (acceld->client->irq) {
		err = i2c_smbus_write_byte_data(acceld->client,
										ACCEL_GRP7_INT_CTRL1,
										acceld->
										accel_registers
										[accel_grp7_int_ctrl]);

		if (err < 0)
			return err;
	}

	if (atomic_read(&acceld->accel_enabled) > 0) {
		err = i2c_smbus_write_byte_data(acceld->client,
										ACCEL_GRP7_CTRL_REG1,
										acceld->
										accel_registers
										[accel_grp7_ctrl_reg1] |
										ACCEL_GRP7_PC1_ON);

		if (err < 0)
			return err;
	} else {
		err = i2c_smbus_write_byte_data(acceld->client,
										ACCEL_GRP7_CTRL_REG1,
										acceld->
										accel_registers
										[accel_grp7_ctrl_reg1]);

		if (err < 0)
			return err;
	}

	return 0;
}

static int kionix_accel_grp7_operate(struct kionix_accel_driver *acceld)
{
	int err;

	err = i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP7_CTRL_REG1,
									acceld->
									accel_registers[accel_grp7_ctrl_reg1] |
									ACCEL_GRP7_PC1_ON);

	if (err < 0)
		return err;

	if (acceld->accel_drdy == 0)
		queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work,
						   0);

	return 0;
}

static int kionix_accel_grp7_standby(struct kionix_accel_driver *acceld)
{
	int err;

	if (acceld->accel_drdy == 0)
		cancel_delayed_work_sync(&acceld->accel_work);

	err =
		i2c_smbus_write_byte_data(acceld->client, ACCEL_GRP7_CTRL_REG1, 0);

	if (err < 0)
		return err;

	return 0;
}

static void kionix_accel_grp7_report_accel_data(struct kionix_accel_driver
		*acceld)
{
	struct {
		union {
			s16 accel_data_s16[3];
			s8 accel_data_s8[6];
		};
	} accel_data;
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = acceld->input_dev;
	int loop;

	/* Only read the output registers if enabled */
	if (atomic_read(&acceld->accel_enabled) > 0) {
		{
			loop = KIONIX_I2C_RETRY_COUNT;

			while (loop) {
				mutex_lock(&input_dev->mutex);
				err =
					kionix_i2c_read(acceld->client,
									ACCEL_GRP7_XOUT_L,
									(u8 *) accel_data.
									accel_data_s16, 6);
				mutex_unlock(&input_dev->mutex);

				if (err < 0) {
					loop--;
					mdelay(KIONIX_I2C_RETRY_TIMEOUT);
				} else
					loop = 0;
			}

			if (err < 0) {
				KMSGERR(&acceld->client->dev,
						"%s: read data output error = %d\n",
						__func__, err);
			} else {
				write_lock(&acceld->rwlock_accel_data);
				x = ((s16)
					 le16_to_cpu(accel_data.
								 accel_data_s16[acceld->
												axis_map_x])) >>
					acceld->shift;
				y = ((s16)
					 le16_to_cpu(accel_data.
								 accel_data_s16[acceld->
												axis_map_y])) >>
					acceld->shift;
				z = ((s16)
					 le16_to_cpu(accel_data.
								 accel_data_s16[acceld->
												axis_map_z])) >>
					acceld->shift;
				acceld->accel_data[acceld->axis_map_x] =
					(acceld->negate_x ? -x : x) +
					acceld->accel_cali[acceld->axis_map_x];
				acceld->accel_data[acceld->axis_map_y] =
					(acceld->negate_y ? -y : y) +
					acceld->accel_cali[acceld->axis_map_y];
				acceld->accel_data[acceld->axis_map_z] =
					(acceld->negate_z ? -z : z) +
					acceld->accel_cali[acceld->axis_map_z];
				{
					input_report_abs(acceld->input_dev,
									 ABS_X,
									 acceld->
									 accel_data[acceld->
												axis_map_x]);
					input_report_abs(acceld->input_dev,
									 ABS_Y,
									 acceld->
									 accel_data[acceld->
												axis_map_y]);
					input_report_abs(acceld->input_dev,
									 ABS_Z,
									 acceld->
									 accel_data[acceld->
												axis_map_z]);
					input_sync(acceld->input_dev);
				}
				write_unlock(&acceld->rwlock_accel_data);
			}
		}
	}

	/* Clear the interrupt if using drdy */
	if (acceld->accel_drdy == 1) {
		loop = KIONIX_I2C_RETRY_COUNT;

		while (loop) {
			err =
				i2c_smbus_read_byte_data(acceld->client,
										 ACCEL_GRP7_INT_REL);

			if (err < 0) {
				loop--;
				mdelay(KIONIX_I2C_RETRY_TIMEOUT);
			} else
				loop = 0;
		}

		if (err < 0)
			KMSGERR(&acceld->client->dev,
					"%s: clear interrupt error = %d\n", __func__,
					err);
	}
}

static void kionix_accel_grp7_update_g_range(struct kionix_accel_driver *acceld)
{
	acceld->accel_registers[accel_grp7_ctrl_reg1] &= ~ACCEL_GRP7_G_MASK;

	switch (acceld->accel_pdata.accel_g_range) {
	case KIONIX_ACCEL_G_8G:
	case KIONIX_ACCEL_G_6G:
		acceld->shift = 2;
		acceld->accel_registers[accel_grp7_ctrl_reg1] |=
			ACCEL_GRP7_G_8G;
		break;

	case KIONIX_ACCEL_G_4G:
		acceld->shift = 3;
		acceld->accel_registers[accel_grp7_ctrl_reg1] |=
			ACCEL_GRP7_G_4G;
		break;

	case KIONIX_ACCEL_G_2G:
	default:
		acceld->shift = 4;
		acceld->accel_registers[accel_grp7_ctrl_reg1] |=
			ACCEL_GRP7_G_2G;
		break;
	}

}

static int kionix_accel_grp7_update_odr(struct kionix_accel_driver *acceld,
										unsigned int poll_interval)
{
	int err;
	int i;
	u8 odr;

	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kionix_accel_grp7_odr_table); i++) {
		odr = kionix_accel_grp7_odr_table[i].mask;

		if (poll_interval < kionix_accel_grp7_odr_table[i].cutoff)
			break;
	}

	/* Do not need to update DATA_CTRL_REG register if the ODR is not changed */
	if (acceld->accel_registers[accel_grp7_data_ctrl] == odr)
		return 0;

	acceld->accel_registers[accel_grp7_data_ctrl] = odr;

	/* Do not need to update DATA_CTRL_REG register if the sensor is not currently turn on */
	if (atomic_read(&acceld->accel_enabled) > 0) {
		err =
			i2c_smbus_write_byte_data(acceld->client,
									  ACCEL_GRP7_CTRL_REG1, 0);

		if (err < 0)
			return err;

		err =
			i2c_smbus_write_byte_data(acceld->client,
									  ACCEL_GRP7_DATA_CTRL,
									  acceld->
									  accel_registers
									  [accel_grp7_data_ctrl]);

		if (err < 0)
			return err;

		err =
			i2c_smbus_write_byte_data(acceld->client,
									  ACCEL_GRP7_CTRL_REG1,
									  acceld->
									  accel_registers
									  [accel_grp7_ctrl_reg1] |
									  ACCEL_GRP7_PC1_ON);

		if (err < 0)
			return err;

		/*############# */
		err =
			i2c_smbus_read_byte_data(acceld->client,
									 ACCEL_GRP7_DATA_CTRL);

		if (err < 0)
			return err;

		switch (err) {
		case ACCEL_GRP7_ODR0_781:
			dev_info(&acceld->client->dev, "ODR = 0.781 Hz\n");
			break;

		case ACCEL_GRP7_ODR1_563:
			dev_info(&acceld->client->dev, "ODR = 1.563 Hz\n");
			break;

		case ACCEL_GRP7_ODR3_125:
			dev_info(&acceld->client->dev, "ODR = 3.125 Hz\n");
			break;

		case ACCEL_GRP7_ODR6_25:
			dev_info(&acceld->client->dev, "ODR = 6.25 Hz\n");
			break;

		case ACCEL_GRP7_ODR12_5:
			dev_info(&acceld->client->dev, "ODR = 12.5 Hz\n");
			break;

		case ACCEL_GRP7_ODR25:
			dev_info(&acceld->client->dev, "ODR = 25 Hz\n");
			break;

		case ACCEL_GRP7_ODR50:
			dev_info(&acceld->client->dev, "ODR = 50 Hz\n");
			break;

		case ACCEL_GRP7_ODR100:
			dev_info(&acceld->client->dev, "ODR = 100 Hz\n");
			break;

		case ACCEL_GRP7_ODR200:
			dev_info(&acceld->client->dev, "ODR = 200 Hz\n");
			break;

		case ACCEL_GRP7_ODR400:
			dev_info(&acceld->client->dev, "ODR = 400 Hz\n");
			break;

		case ACCEL_GRP7_ODR800:
			dev_info(&acceld->client->dev, "ODR = 800 Hz\n");
			break;

		case ACCEL_GRP7_ODR1600:
			dev_info(&acceld->client->dev, "ODR = 1600 Hz\n");
			break;

		default:
			dev_info(&acceld->client->dev, "Unknown ODR\n");
			break;
		}

		/*############# */
	}

	return 0;
}

static int kionix_accel_power_on(struct kionix_accel_driver *acceld)
{
	if (acceld->accel_pdata.power_on)
		return acceld->accel_pdata.power_on();

	return 0;
}

static void kionix_accel_power_off(struct kionix_accel_driver *acceld)
{
	if (acceld->accel_pdata.power_off)
		acceld->accel_pdata.power_off();
}

static irqreturn_t kionix_accel_isr(int irq, void *dev)
{

/* disable_irq_nosync(irq); */
#if USE_WAIT_QUEUE
	gsensor_flag = 1;
	wake_up_interruptible(&waiter);
#else
	/*queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work, 0); */
	acceld->kionix_accel_report_accel_data(acceld);
#endif
	return IRQ_HANDLED;
}

static void kionix_accel_work(struct work_struct *work)
{
	struct kionix_accel_driver *acceld =
		container_of((struct delayed_work *)work,
					 struct kionix_accel_driver, accel_work);

	/*KMSGINF(&acceld->client->dev, "<%s> start L%d.\n", __func__, __LINE__); */
	if (acceld->accel_drdy == 0)
		queue_delayed_work(acceld->accel_workqueue, &acceld->accel_work,
						   acceld->poll_delay);

	acceld->kionix_accel_report_accel_data(acceld);
}

#if USE_WAIT_QUEUE
static void kionix_acc_update_data_func(void)
{
	struct kionix_accel_driver *acceld = g_acceld;

	acceld->kionix_accel_report_accel_data(acceld);
	/*		enable_irq(acc->irq1); */
}

static int kionix_acc_event_handler(void *unused)
{
	struct sched_param param = {.sched_priority = 5 };

	sched_setscheduler(current, SCHED_RR, &param);

	do {
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, (gsensor_flag != 0));
		gsensor_flag = 0;
		set_current_state(TASK_RUNNING);
		kionix_acc_update_data_func();
	} while (!kthread_should_stop());

	return 0;
}
#endif
static void kionix_accel_update_direction(struct kionix_accel_driver *acceld)
{
	unsigned int direction = acceld->accel_pdata.accel_direction;
	unsigned int accel_group = acceld->accel_group;

	write_lock(&acceld->rwlock_accel_data);
	acceld->axis_map_x = ((direction - 1) % 2);
	acceld->axis_map_y = (direction % 2);
	acceld->axis_map_z = 2;
	acceld->negate_z = ((direction - 1) / 4);

	switch (accel_group) {
	case KIONIX_ACCEL_GRP3:
	case KIONIX_ACCEL_GRP6:
		acceld->negate_x = (((direction + 2) / 2) % 2);
		acceld->negate_y = (((direction + 5) / 4) % 2);
		break;

	case KIONIX_ACCEL_GRP5:
		acceld->axis_map_x = (direction % 2);
		acceld->axis_map_y = ((direction - 1) % 2);
		acceld->negate_x = (((direction + 1) / 2) % 2);
		acceld->negate_y =
			(((direction / 2) + ((direction - 1) / 4)) % 2);
		break;

	default:
		acceld->negate_x = ((direction / 2) % 2);
		acceld->negate_y = (((direction + 1) / 4) % 2);
		break;
	}

	write_unlock(&acceld->rwlock_accel_data);
}

static int kionix_accel_enable(struct kionix_accel_driver *acceld)
{
	int err = 0;

	KMSGINF(&acceld->client->dev, "kionix_accel_enable\n");
	mutex_lock(&acceld->mutex_enable);
	/* Make sure that the sensor had successfully resumed before enabling it */
	err = acceld->kionix_accel_operate(acceld);

	if (err < 0) {
		KMSGERR(&acceld->client->dev,
				"%s: kionix_accel_operate returned err = %d\n",
				__func__, err);
		goto exit;
	}

exit:
	mutex_unlock(&acceld->mutex_enable);
	return err;
}

static int kionix_accel_disable(struct kionix_accel_driver *acceld)
{
	int err = 0;

	KMSGINF(&acceld->client->dev, "kionix_accel_disable\n");
	/*	  kionix_accel_enable(acceld); */
	mutex_lock(&acceld->mutex_enable);
	err = acceld->kionix_accel_standby(acceld);

	if (err < 0) {
		KMSGERR(&acceld->client->dev,
				"%s: kionix_accel_standby returned err = %d\n",
				__func__, err);
		goto exit;
	}

exit:
	mutex_unlock(&acceld->mutex_enable);
	return err;
}

static int kionix_accel_input_open(struct input_dev *input)
{
	struct kionix_accel_driver *acceld = input_get_drvdata(input);

	KMSGINF(&acceld->client->dev, "kionix_accel_input_open.\n");
	return 0;
}

static void kionix_accel_input_close(struct input_dev *dev)
{
	struct kionix_accel_driver *acceld = input_get_drvdata(dev);

	KMSGINF(&acceld->client->dev, "kionix_accel_input_close.\n");
}

static void kionix_accel_init_input_device(struct kionix_accel_driver *acceld,
		struct input_dev *input_dev)
{
	input_dev->name = KIONIX_ACCEL_INPUT_NAME;	/*KIONIX_ACCEL_NAME; */
	input_dev->id.bustype = BUS_I2C;

	/* input_dev->dev.parent = &acceld->client->dev; */
	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_X, -ACCEL_G_MAX, ACCEL_G_MAX,
						 ACCEL_FUZZ, ACCEL_FLAT);
	input_set_abs_params(input_dev, ABS_Y, -ACCEL_G_MAX, ACCEL_G_MAX,
						 ACCEL_FUZZ, ACCEL_FLAT);
	input_set_abs_params(input_dev, ABS_Z, -ACCEL_G_MAX, ACCEL_G_MAX,
						 ACCEL_FUZZ, ACCEL_FLAT);
}

static int kionix_accel_setup_input_device(struct kionix_accel_driver *acceld)
{
	struct input_dev *input_dev;
	int err;

	input_dev = input_allocate_device();

	if (!input_dev) {
		KMSGERR(&acceld->client->dev, "input_allocate_device failed\n");
		return -ENOMEM;
	}

	acceld->input_dev = input_dev;
	input_dev->open = kionix_accel_input_open;
	input_dev->close = kionix_accel_input_close;
	input_set_drvdata(input_dev, acceld);
	kionix_accel_init_input_device(acceld, input_dev);
	err = input_register_device(acceld->input_dev);

	if (err) {
		KMSGERR(&acceld->client->dev,
				"%s: input_register_device returned err = %d\n",
				__func__, err);
		input_free_device(acceld->input_dev);
		return err;
	}

	return 0;
}

/* Returns the enable state of device */
static ssize_t kionix_accel_get_enable(struct device *dev,
									   struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n",
				   atomic_read(&acceld->accel_enabled) > 0 ? 1 : 0);
}

/* Allow users to enable/disable the device */
static ssize_t kionix_accel_set_enable(struct device *dev,
									   struct device_attribute *attr,
									   const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&acceld->accel_enabled);
	struct input_dev *input_dev = acceld->input_dev;
	char *buf2;
	const int enable_count = 1;
	unsigned long enable;
	int err = 0;
	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	if (kionix_strtok(buf, count, &buf2, enable_count) < 0) {
		KMSGERR(&acceld->client->dev,
				"%s: No enable data being read. "
				"No enable data will be updated.\n", __func__);
	} else {
		/* Removes any leading negative sign */
		while (*buf2 == '-')
			buf2++;

		err =
			kstrtouint((const char *)buf2, 10, (unsigned int *)&enable);

		if (err < 0) {
			KMSGERR(&acceld->client->dev,
					"%s: kstrtouint returned err = %d\n", __func__,
					err);
			goto exit;
		}

		if (enable && !pre_enable) {
			kionix_accel_enable(acceld);
			atomic_set(&acceld->accel_enabled, 1);
		} else if (!enable && pre_enable) {
			kionix_accel_disable(acceld);
			atomic_set(&acceld->accel_enabled, 0);
		}
	}

exit:
	mutex_unlock(&input_dev->mutex);
	return (err < 0) ? err : count;
}

/* Returns currently selected poll interval (in ms) */
static ssize_t kionix_accel_get_delay(struct device *dev,
									  struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", acceld->poll_interval);
}

/* Allow users to select a new poll interval (in ms) */
static ssize_t kionix_accel_set_delay(struct device *dev,
									  struct device_attribute *attr,
									  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	struct input_dev *input_dev = acceld->input_dev;

	unsigned long interval;
	int err = 0;

	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	err = kstrtoul(buf, 10, &interval);
	if (err)
		return err;

	if (err < 0) {
		KMSGERR(&acceld->client->dev,
				"%s: kstrtouint returned err = %d\n", __func__, err);
			goto exit;
		}

	if (acceld->accel_drdy == 1)
		disable_irq(client->irq);

	/*
	 * Set current interval to the greater of the minimum interval or
	 * the requested interval
	 */
	acceld->poll_interval =
		max((unsigned int)interval,
			acceld->accel_pdata.min_interval);
	/* acceld->poll_delay = msecs_to_jiffies(acceld->poll_interval); */
	err =
		acceld->kionix_accel_update_odr(acceld, acceld->poll_interval);

	if (acceld->accel_drdy == 1)
		enable_irq(client->irq);


exit:
	mutex_unlock(&input_dev->mutex);
	return (err < 0) ? err : count;
}

/* Returns the direction of device */
static ssize_t kionix_accel_get_direct(struct device *dev,
									   struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", acceld->accel_pdata.accel_direction);
}

/* Allow users to change the direction the device */
static ssize_t kionix_accel_set_direct(struct device *dev,
									   struct device_attribute *attr,
									   const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	struct input_dev *input_dev = acceld->input_dev;
	char *buf2;
	const int direct_count = 1;
	unsigned long direction;
	int err = 0;
	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	if (kionix_strtok(buf, count, &buf2, direct_count) < 0) {
		KMSGERR(&acceld->client->dev,
				"%s: No direction data being read. "
				"No direction data will be updated.\n", __func__);
	} else {
		/* Removes any leading negative sign */
		while (*buf2 == '-')
			buf2++;

		err =
			kstrtouint((const char *)buf2, 10,
					   (unsigned int *)&direction);

		if (err < 0) {
			KMSGERR(&acceld->client->dev,
					"%s: kstrtouint returned err = %d\n", __func__,
					err);
			goto exit;
		}

		if (direction < 1 || direction > 8) {
			KMSGERR(&acceld->client->dev,
					"%s: invalid direction = %d\n", __func__,
					(unsigned int)direction);
		} else {
			acceld->accel_pdata.accel_direction = (u8) direction;
			kionix_accel_update_direction(acceld);
		}
	}

exit:
	mutex_unlock(&input_dev->mutex);
	return (err < 0) ? err : count;
}

/* Returns the data output of device */
static ssize_t kionix_accel_get_data(struct device *dev,
									 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	int x, y, z;

	read_lock(&acceld->rwlock_accel_data);
	x = acceld->accel_data[acceld->axis_map_x];
	y = acceld->accel_data[acceld->axis_map_y];
	z = acceld->accel_data[acceld->axis_map_z];
	read_unlock(&acceld->rwlock_accel_data);
	return sprintf(buf, "%d %d %d\n", x, y, z);
}

/* Returns the calibration value of the device */
static ssize_t kionix_accel_get_cali(struct device *dev,
									 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	int calibration[3];

	read_lock(&acceld->rwlock_accel_data);
	calibration[0] = acceld->accel_cali[acceld->axis_map_x];
	calibration[1] = acceld->accel_cali[acceld->axis_map_y];
	calibration[2] = acceld->accel_cali[acceld->axis_map_z];
	read_unlock(&acceld->rwlock_accel_data);
	return sprintf(buf, "%d %d %d\n", calibration[0], calibration[1],
				   calibration[2]);
}

/* Allow users to change the calibration value of the device */
static ssize_t kionix_accel_set_cali(struct device *dev,
									 struct device_attribute *attr,
									 const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	struct input_dev *input_dev = acceld->input_dev;
	const int cali_count = 3;	/* How many calibration that we expect to get from the string */
	char **buf2;
	long calibration[cali_count];
	int err = 0, i = 0;
	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);
	buf2 = kcalloc(cali_count, sizeof(char *), GFP_KERNEL);

	if (kionix_strtok(buf, count, buf2, cali_count) < 0) {
		KMSGERR(&acceld->client->dev,
				"%s: Not enough calibration data being read. "
				"No calibration data will be updated.\n", __func__);
	} else {
		/* Convert string to integers  */
		for (i = 0; i < cali_count; i++) {
			err =
				kstrtoint((const char *) *(buf2 + i), 10,
						  (int *)&calibration[i]);

			if (err < 0) {
				KMSGERR(&acceld->client->dev,
						"%s: kstrtoint returned err = %d."
						"No calibration data will be updated.\n",
						__func__, err);
				goto exit;
			}
		}

		write_lock(&acceld->rwlock_accel_data);
		acceld->accel_cali[acceld->axis_map_x] = (int)calibration[0];
		acceld->accel_cali[acceld->axis_map_y] = (int)calibration[1];
		acceld->accel_cali[acceld->axis_map_z] = (int)calibration[2];
		write_unlock(&acceld->rwlock_accel_data);
	}

exit:

	for (i = 0; i < cali_count; i++)
		kfree(*(buf2 + i));

	kfree(buf2);
	mutex_unlock(&input_dev->mutex);
	return (err < 0) ? err : count;
}

static ssize_t kionix_accel_name_show(struct device *dev,
									  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", KIONIX_ACCEL_VENDOR_NAME);
}

static ssize_t kionix_accel_chip_info_show(struct device *dev,
									  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", chip_info);
}

static DEVICE_ATTR(enable, 0660, kionix_accel_get_enable,
				   kionix_accel_set_enable);
static DEVICE_ATTR(delay, 0660, kionix_accel_get_delay, kionix_accel_set_delay);
static DEVICE_ATTR(direct, 0660, kionix_accel_get_direct,
				   kionix_accel_set_direct);
static DEVICE_ATTR(data, 0440, kionix_accel_get_data, NULL);
static DEVICE_ATTR(cali, 0660, kionix_accel_get_cali, kionix_accel_set_cali);
static DEVICE_ATTR(vendor_name, 0440, kionix_accel_name_show, NULL);
static DEVICE_ATTR(chip_info, 0440, kionix_accel_chip_info_show, NULL);
static struct attribute *kionix_accel_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_direct.attr,
	&dev_attr_data.attr,
	&dev_attr_cali.attr,
	&dev_attr_vendor_name.attr,
	&dev_attr_chip_info.attr,
	NULL
};

static struct attribute_group kionix_accel_attribute_group = {
	.attrs = kionix_accel_attributes
};

static int kionix_verify(struct kionix_accel_driver *acceld)
{
	int retval = i2c_smbus_read_byte_data(acceld->client, ACCEL_WHO_AM_I);
#if KIONIX_KMSG_INF

	switch (retval) {
	case KIONIX_ACCEL_WHO_AM_I_KXTE9:
#ifdef CONFIG_GET_DEVICES_VENDOR_INFO
		strcpy(g_sensor_ic_info, "KXTE9");
#endif
		KMSGINF(&acceld->client->dev,
				"this accelerometer is a KXTE9.\n");
		break;

	case KIONIX_ACCEL_WHO_AM_I_KXTF9:
#ifdef CONFIG_GET_DEVICES_VENDOR_INFO
		strcpy(g_sensor_ic_info, "KXTF9");
#endif
		KMSGINF(&acceld->client->dev,
				"this accelerometer is a KXTF9.\n");
		break;

	case KIONIX_ACCEL_WHO_AM_I_KXTI9_1001:
#ifdef CONFIG_GET_DEVICES_VENDOR_INFO
		strcpy(g_sensor_ic_info, "KXTF9");
#endif
		KMSGINF(&acceld->client->dev,
				"this accelerometer is a KXTI9-1001.\n");
		break;

	case KIONIX_ACCEL_WHO_AM_I_KXTIK_1004:
#ifdef CONFIG_GET_DEVICES_VENDOR_INFO
		strcpy(g_sensor_ic_info, "KXTF9");
#endif
		KMSGINF(&acceld->client->dev,
				"this accelerometer is a KXTIK-1004.\n");
		break;

	case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1005:
#ifdef CONFIG_GET_DEVICES_VENDOR_INFO
		strcpy(g_sensor_ic_info, "KXTF9");
#endif
		KMSGINF(&acceld->client->dev,
				"this accelerometer is a KXTJ9-1005.\n");
		break;

	case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1007:
#ifdef CONFIG_GET_DEVICES_VENDOR_INFO
		strcpy(g_sensor_ic_info, "KXTF9");
#endif
		KMSGINF(&acceld->client->dev,
				"this accelerometer is a KXTJ9-1007.\n");
		break;

	case KIONIX_ACCEL_WHO_AM_I_KXCJ9_1008:
#ifdef CONFIG_GET_DEVICES_VENDOR_INFO
		strcpy(g_sensor_ic_info, "KXCJ9-1008");
#endif
		KMSGINF(&acceld->client->dev,
				"this accelerometer is a KXCJ9-1008.\n");
		break;

	case KIONIX_ACCEL_WHO_AM_I_KXTJ2_1009:
#ifdef CONFIG_GET_DEVICES_VENDOR_INFO
		strcpy(g_sensor_ic_info, "KXTJ2-1009");
#endif
		KMSGINF(&acceld->client->dev,
				"this accelerometer is a KXTJ2-1009.\n");
		break;

	case KIONIX_ACCEL_WHO_AM_I_KXCJK_1013:
#ifdef CONFIG_GET_DEVICES_VENDOR_INFO
		strcpy(g_sensor_ic_info, "KXCJK-1013");
#endif
		KMSGINF(&acceld->client->dev,
				"this accelerometer is a KXCJK-1013.\n");
		break;

	case KIONIX_ACCEL_WHO_AM_I_KX023:
#ifdef CONFIG_GET_DEVICES_VENDOR_INFO
		strcpy(g_sensor_ic_info, "KX023");
#endif
		KMSGINF(&acceld->client->dev,
				"this accelerometer is a KX023.\n");
		break;

	default:
		break;
	}

#endif
	return retval;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void kionix_accel_earlysuspend_suspend(struct early_suspend *h)
{
	struct kionix_accel_driver *acceld =
		container_of(h, struct kionix_accel_driver, early_suspend);

	if (atomic_read(&acceld->accel_enabled))
		kionix_accel_disable(acceld);
}

void kionix_accel_earlysuspend_resume(struct early_suspend *h)
{
	struct kionix_accel_driver *acceld =
		container_of(h, struct kionix_accel_driver, early_suspend);

	if (atomic_read(&acceld->accel_enabled))
		kionix_accel_enable(acceld);
}
#endif /* CONFIG_HAS_EARLYSUSPEND */
static ssize_t kionix_tool_read(struct file *page, char __user *start,
								size_t off, loff_t *eof)
{
	return 1;
}

static ssize_t kionix_tool_write(struct file *filp, const char __user *buff,
							 size_t len, loff_t *data)
{
	return 1;
}

static const struct file_operations kionix_ops = {
	.owner = THIS_MODULE,
	.read = kionix_tool_read,
	.write = kionix_tool_write,
};

#ifdef CONFIG_OF
static struct kionix_accel_platform_data *kionix_accel_parse_dt(struct device
		*dev)
{
	struct kionix_accel_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int ret;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);

	if (!pdata) {
		dev_err(dev,
				"Could not allocate struct kionix_accel_platform_data");
		return NULL;
	}

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

	ret = of_property_read_u32(np, "g_range", &pdata->accel_g_range);

	if (ret) {
		dev_err(dev, "fail to get g_range\n");
		goto fail;
	}

	ret = of_property_read_u32(np, "accel_res", &pdata->accel_res);

	if (ret) {
		dev_err(dev, "fail to get accel_res\n");
		goto fail;
	}

	ret =
		of_property_read_u32(np, "accel_irq_use_drdy",
							 &pdata->accel_irq_use_drdy);

	if (ret) {
		dev_err(dev, "fail to get accel_irq_use_drdy\n");
		goto fail;
	}

	ret =
		of_property_read_u32(np, "accel_direction",
							 &pdata->accel_direction);

	if (ret) {
		dev_err(dev, "fail to get accel_direction\n");
		goto fail;
	}

	pdata->irq_gpio_number = of_get_gpio(np, 0);

	if (pdata->irq_gpio_number < 0) {
		dev_err(dev, "fail to get irq_gpio_number\n");
		goto fail;
	}

	return pdata;
fail:
	kfree(pdata);
	return NULL;
}
#endif /* CONFIG_OF */
static int kionix_acc_misc_open(struct inode *inode, struct file *file)
{
	int err;

	err = nonseekable_open(inode, file);

	if (err < 0)
		return err;

	file->private_data = g_acceld;
	return 0;
}

static long kionix_acc_misc_ioctl(struct file *file, unsigned int cmd,
								  unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int err = 0;
	int interval = 2;
	char enable;
	unsigned long delay;
	int xyz[3] = { 0 };
	int pre_enable;
	struct kionix_accel_driver *acceld = file->private_data;

	printk("%s cmd:[%d]\n", __FUNCTION__, cmd);

	switch (cmd) {
	case KIONIX_ACCEL_IOCTL_SET_ENABLE: {
		printk("KIONIX_ACCEL_IOCTL_SET_ENABLE\n");
		pre_enable = atomic_read(&acceld->accel_enabled);

		if (copy_from_user(&enable, argp, sizeof(enable)))
			return -EFAULT;

		printk("%s enable= %d\n", __FUNCTION__, enable);

		if (g_acceld == NULL) {
			printk
			("kionix_acc_misc_ioctl error [NULL==g_acceld] !!");
			return -EINVAL;
		}

		if (enable && !pre_enable) {
			kionix_accel_enable(acceld);
			atomic_set(&acceld->accel_enabled, 1);
		} else if (!enable && pre_enable) {
			kionix_accel_disable(acceld);
			atomic_set(&acceld->accel_enabled, 0);
		}

		break;
	}

	case KIONIX_ACCEL_IOCTL_GET_ENABLE:
		printk("KIONIX_ACCEL_IOCTL_GET_ENABLE\n");
		interval = atomic_read(&acceld->accel_enabled) > 0 ? 1 : 0;

		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EINVAL;

		break;

	case KIONIX_ACCEL_IOCTL_SET_DELAY:
		if (copy_from_user(&delay, argp, sizeof(delay)))
			return -EINVAL;

		/*		if(acceld->accel_drdy == 1) */
		/*				disable_irq(acceld->client->irq); */
		printk("KIONIX_ACCEL_IOCTL_SET_DELAY %lu\n", delay);
		/*
		 * Set current interval to the greater of the minimum interval or
		 * the requested interval
		 */
		acceld->poll_interval =
			max((unsigned int)delay, acceld->accel_pdata.min_interval);
		acceld->poll_delay = msecs_to_jiffies(acceld->poll_interval);
		err =
			acceld->kionix_accel_update_odr(acceld,
											acceld->poll_interval);
		/*		if(acceld->accel_drdy == 1) */
		/*				enable_irq(acceld->client->irq); */
		break;

	case KIONIX_ACCEL_IOCTL_GET_DELAY:
		if (copy_to_user
			(argp, &acceld->poll_interval,
			 sizeof(acceld->poll_interval)))
			return -EINVAL;

		break;

	case KIONIX_ACC_IOCTL_GET_XYZ:
		printk("%s get xyz\n", __func__);

		if (NULL == g_acceld) {
			printk
			("kionix_acc_misc_ioctl error [NULL==g_acceld] !!");
			return -EINVAL;
		}

		xyz[0] =
			g_acceld->accel_data[g_acceld->axis_map_x] * 98 * 100 /
			1024;
		xyz[1] =
			g_acceld->accel_data[g_acceld->axis_map_y] * 98 * 100 /
			1024;
		xyz[2] =
			g_acceld->accel_data[g_acceld->axis_map_z] * 98 * 100 /
			1024;
		coordinate_data_convert(xyz, ORI);

		if (copy_to_user(argp, xyz, sizeof(xyz))) {
			pr_err(" %s %d error in copy_to_user \n",
				   __func__, __LINE__);
			return -EINVAL;
		}

		break;

	case KIONIX_ACC_IOCTL_GET_CHIP_ID: {
		u8 devid = (u8) g_chip_id;
		u8 devinfo[DEVICE_INFO_LEN] = { 0 };
		sprintf(devinfo, "%s, %#x", DEVICE_INFO, devid);

		if (copy_to_user(argp, devinfo, sizeof(devinfo))) {
			printk
			("%s error in copy_to_user(IOCTL_GET_CHIP_ID)\n",
			 __func__);
			return -EINVAL;
		}

		break;
	}

	default:
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations kionix_acc_misc_fops = {
	.owner = THIS_MODULE,
	.open = kionix_acc_misc_open,
	.unlocked_ioctl = kionix_acc_misc_ioctl,
};

static struct miscdevice kionix_acc_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = KIONIX_ACC_DEV_NAME,
	.fops = &kionix_acc_misc_fops,
};

extern char *hq_get_gsensor_name(char *buf);
static int kionix_accel_probe(struct i2c_client *client,
							  const struct i2c_device_id *id)
{
	struct kionix_accel_platform_data *accel_pdata =
				client->dev.platform_data;
	struct kionix_accel_driver *acceld;
	int err;
	struct proc_dir_entry *proc_dir, *proc_entry;
#ifdef CONFIG_OF
	struct device_node *np = client->dev.of_node;

	if (np && !accel_pdata) {
		accel_pdata = kionix_accel_parse_dt(&client->dev);

		if (accel_pdata) {
			client->dev.platform_data = accel_pdata;
		}

		if (!accel_pdata)
			return -ENOMEM;

	}

#endif /* CONFIG_OF */
	KMSGINF(&client->dev, "<%s> start L%d.\n", __func__, __LINE__);
	err = gpio_request(accel_pdata->irq_gpio_number, KIONIX_ACCEL_IRQ_PIN);

	if (err) {
		KMSGERR(&client->dev, "gpio_request failed!\n");
		goto err_gpio_request_failed;
	}

	gpio_direction_input(accel_pdata->irq_gpio_number);
	client->irq = gpio_to_irq(accel_pdata->irq_gpio_number);

	if (!i2c_check_functionality(client->adapter,
								 I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE_DATA)) {
		KMSGERR(&client->dev, "client is not i2c capable. Abort.\n");
		err = -ENXIO;
		goto err_i2c_check_func;
	}

	acceld = kzalloc(sizeof(*acceld), GFP_KERNEL);

	if (acceld == NULL) {
		KMSGERR(&client->dev,
				"failed to allocate memory for module data. Abort.\n");
		err = -ENOMEM;
		goto err_malloc_acceld;
	}

	acceld->client = client;
	acceld->accel_pdata = *accel_pdata;
	i2c_set_clientdata(client, acceld);
	g_acceld = acceld;
	err = kionix_accel_power_on(acceld);

	if (err < 0)
		goto err_free_mem;

	if (accel_pdata->init) {
		err = accel_pdata->init();

		if (err < 0)
			goto err_accel_pdata_power_off;
	}

	err = kionix_verify(acceld);

	if (err < 0) {
		KMSGERR(&acceld->client->dev,
				"%s: addr[0x%x] kionix_verify returned err = %d. Abort.\n",
				__func__, client->addr, err);
		goto err_accel_pdata_exit;
	}

	g_chip_id = err;
#ifdef CONFIG_GET_DEVICES_VENDOR_INFO
	strcpy(g_sensor_vendor_info, "Kionix_1st");
#endif
	KMSGINF(&client->dev, "%s: kionix accelerometer chip id = 0x%x.\n",
			__func__, err);

	/* Setup group specific configuration and function callback */
	switch (err) {
	case KIONIX_ACCEL_WHO_AM_I_KXTE9:
		acceld->accel_group = KIONIX_ACCEL_GRP1;
		acceld->accel_registers =
			kzalloc(sizeof(u8) * accel_grp1_regs_count, GFP_KERNEL);

		if (acceld->accel_registers == NULL) {
			KMSGERR(&client->dev,
					"failed to allocate memory for accel_registers. Abort.\n");
			goto err_accel_pdata_exit;
		}

		acceld->accel_drdy = 0;
		acceld->kionix_accel_report_accel_data =
			kionix_accel_grp1_report_accel_data;
		acceld->kionix_accel_update_odr = kionix_accel_grp1_update_odr;
		acceld->kionix_accel_power_on_init =
			kionix_accel_grp1_power_on_init;
		acceld->kionix_accel_operate = kionix_accel_grp1_operate;
		acceld->kionix_accel_standby = kionix_accel_grp1_standby;
		break;

	case KIONIX_ACCEL_WHO_AM_I_KXTF9:
	case KIONIX_ACCEL_WHO_AM_I_KXTI9_1001:
	case KIONIX_ACCEL_WHO_AM_I_KXTIK_1004:
	case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1005:
		if (err == KIONIX_ACCEL_WHO_AM_I_KXTIK_1004)
			acceld->accel_group = KIONIX_ACCEL_GRP3;
		else
			acceld->accel_group = KIONIX_ACCEL_GRP2;

		sprintf(chip_info, "%s", "kionix_accel_grp2");

		acceld->accel_registers =
			kzalloc(sizeof(u8) * accel_grp2_regs_count, GFP_KERNEL);

		if (acceld->accel_registers == NULL) {
			KMSGERR(&client->dev,
					"failed to allocate memory for accel_registers. Abort.\n");
			goto err_accel_pdata_exit;
		}

		switch (acceld->accel_pdata.accel_res) {
		case KIONIX_ACCEL_RES_6BIT:
		case KIONIX_ACCEL_RES_8BIT:
			acceld->accel_registers[accel_grp2_ctrl_reg1] |=
				ACCEL_GRP2_RES_8BIT;
			break;

		case KIONIX_ACCEL_RES_12BIT:
		case KIONIX_ACCEL_RES_16BIT:
		default:
			acceld->accel_registers[accel_grp2_ctrl_reg1] |=
				ACCEL_GRP2_RES_12BIT;
			break;
		}

		if (acceld->accel_pdata.accel_irq_use_drdy && client->irq) {
			acceld->accel_registers[accel_grp2_int_ctrl] |=
				ACCEL_GRP2_IEN | ACCEL_GRP2_IEA;
			acceld->accel_registers[accel_grp2_ctrl_reg1] |=
				ACCEL_GRP2_DRDYE;
			acceld->accel_drdy = 1;
		} else
			acceld->accel_drdy = 0;

		kionix_accel_grp2_update_g_range(acceld);
		acceld->kionix_accel_report_accel_data =
			kionix_accel_grp2_report_accel_data;
		acceld->kionix_accel_update_odr = kionix_accel_grp2_update_odr;
		acceld->kionix_accel_power_on_init =
			kionix_accel_grp2_power_on_init;
		acceld->kionix_accel_operate = kionix_accel_grp2_operate;
		acceld->kionix_accel_standby = kionix_accel_grp2_standby;
		break;

	case KIONIX_ACCEL_WHO_AM_I_KXTJ9_1007:
	case KIONIX_ACCEL_WHO_AM_I_KXCJ9_1008:
	case KIONIX_ACCEL_WHO_AM_I_KXTJ2_1009:
	case KIONIX_ACCEL_WHO_AM_I_KXTJ3_105X:
	case KIONIX_ACCEL_WHO_AM_I_KXCJK_1013:
		if ((err == KIONIX_ACCEL_WHO_AM_I_KXTJ2_1009)
			|| (err == KIONIX_ACCEL_WHO_AM_I_KXTJ3_105X))
			acceld->accel_group = KIONIX_ACCEL_GRP5;
		else if (err == KIONIX_ACCEL_WHO_AM_I_KXCJK_1013)
			acceld->accel_group = KIONIX_ACCEL_GRP6;
		else
			acceld->accel_group = KIONIX_ACCEL_GRP4;

		acceld->accel_registers =
			kzalloc(sizeof(u8) * accel_grp4_regs_count, GFP_KERNEL);

		if (acceld->accel_registers == NULL) {
			KMSGERR(&client->dev,
					"failed to allocate memory for accel_registers. Abort.\n");
			goto err_accel_pdata_exit;
		}

		switch (acceld->accel_pdata.accel_res) {
		case KIONIX_ACCEL_RES_6BIT:
		case KIONIX_ACCEL_RES_8BIT:
			acceld->accel_registers[accel_grp4_ctrl_reg1] |=
				ACCEL_GRP4_RES_8BIT;
			break;

		case KIONIX_ACCEL_RES_12BIT:
		case KIONIX_ACCEL_RES_16BIT:
		default:
			acceld->accel_registers[accel_grp4_ctrl_reg1] |=
				ACCEL_GRP4_RES_12BIT;
			break;
		}

		if (acceld->accel_pdata.accel_irq_use_drdy && client->irq) {
			acceld->accel_registers[accel_grp4_int_ctrl] |=
				ACCEL_GRP4_IEN | ACCEL_GRP4_IEA;
			acceld->accel_registers[accel_grp4_ctrl_reg1] |=
				ACCEL_GRP4_DRDYE;
			acceld->accel_drdy = 1;
		} else
			acceld->accel_drdy = 0;

		kionix_accel_grp4_update_g_range(acceld);
		acceld->kionix_accel_report_accel_data =
			kionix_accel_grp4_report_accel_data;
		acceld->kionix_accel_update_odr = kionix_accel_grp4_update_odr;
		acceld->kionix_accel_power_on_init =
			kionix_accel_grp4_power_on_init;
		acceld->kionix_accel_operate = kionix_accel_grp4_operate;
		acceld->kionix_accel_standby = kionix_accel_grp4_standby;
		break;

	case KIONIX_ACCEL_WHO_AM_I_KX023:
		acceld->accel_registers =
			kzalloc(sizeof(u8) * accel_grp7_regs_count, GFP_KERNEL);

		if (acceld->accel_registers == NULL) {
			KMSGERR(&client->dev,
					"failed to allocate memory for accel_registers. Abort.\n");
			goto err_accel_pdata_exit;
		}

		switch (acceld->accel_pdata.accel_res) {
		case KIONIX_ACCEL_RES_6BIT:
		case KIONIX_ACCEL_RES_8BIT:
			acceld->accel_registers[accel_grp7_ctrl_reg1] |=
				ACCEL_GRP7_RES_8BIT;
			break;

		case KIONIX_ACCEL_RES_12BIT:
		case KIONIX_ACCEL_RES_16BIT:
		default:
			acceld->accel_registers[accel_grp7_ctrl_reg1] |=
				ACCEL_GRP7_RES_16BIT;
			break;
		}

		if (acceld->accel_pdata.accel_irq_use_drdy && client->irq) {
			acceld->accel_registers[accel_grp7_int_ctrl] |=
				ACCEL_GRP7_IEN | ACCEL_GRP7_IEA;
			acceld->accel_registers[accel_grp7_ctrl_reg1] |=
				ACCEL_GRP7_DRDYE;
			acceld->accel_drdy = 1;
		} else
			acceld->accel_drdy = 0;

		kionix_accel_grp7_update_g_range(acceld);
		acceld->kionix_accel_report_accel_data =
			kionix_accel_grp7_report_accel_data;
		acceld->kionix_accel_update_odr = kionix_accel_grp7_update_odr;
		acceld->kionix_accel_power_on_init =
			kionix_accel_grp7_power_on_init;
		acceld->kionix_accel_operate = kionix_accel_grp7_operate;
		acceld->kionix_accel_standby = kionix_accel_grp7_standby;
		break;

	default:
		KMSGERR(&acceld->client->dev,
				"%s: unsupported device, who am i = %d. Abort.\n",
				__func__, err);
		goto err_accel_pdata_exit;
	}

	err = kionix_accel_setup_input_device(acceld);

	if (err)
		goto err_free_accel_registers;

	atomic_set(&acceld->accel_enabled, 0);
	mutex_init(&acceld->mutex_enable);
	rwlock_init(&acceld->rwlock_accel_data);
	acceld->on_before_suspend = 0;
	acceld->poll_interval = acceld->accel_pdata.poll_interval;
	acceld->poll_delay = msecs_to_jiffies(acceld->poll_interval);
	acceld->kionix_accel_update_odr(acceld, acceld->poll_interval);
	kionix_accel_update_direction(acceld);
	proc_dir = proc_mkdir("sensors", NULL);

	if (proc_dir == NULL)
		KMSGERR(&client->dev, "failed to create /proc/sensors\n");
	else {
		proc_entry = proc_create("accelinfo", 0644, proc_dir, &kionix_ops);	/*create_proc_entry( "accelinfo", 0644, proc_dir); */

		if (proc_entry == NULL)
			KMSGERR(&client->dev,
					"failed to create /proc/cpu/accelinfo\n");
	}

	acceld->accel_workqueue = create_workqueue("Kionix Accel Workqueue");
	INIT_DELAYED_WORK(&acceld->accel_work, kionix_accel_work);

	if (acceld->accel_drdy) {
		err = request_irq(client->irq, kionix_accel_isr,
						  IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND,
						  KIONIX_ACCEL_IRQ, acceld);

		if (err) {
			KMSGERR(&acceld->client->dev,
					"%s: request_threaded_irq returned err = %d\n",
					__func__, err);
			KMSGERR(&acceld->client->dev,
					"%s: running in software polling mode instead\n",
					__func__);
			acceld->accel_drdy = 0;
		}

		KMSGINF(&acceld->client->dev,
				"running in hardware interrupt mode\n");
	} else {
		KMSGINF(&acceld->client->dev,
				"running in software polling mode\n");
	}

#if USE_WAIT_QUEUE
	thread = kthread_run(kionix_acc_event_handler, 0, "kionix-thread");

	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		KMSGERR(&acceld->client->dev,
				"failed to create kernel thread: %d\n", err);
	}

#endif
	err = acceld->kionix_accel_power_on_init(acceld);

	if (err) {
		KMSGERR(&acceld->client->dev,
				"%s: kionix_accel_power_on_init returned err = %d. Abort.\n",
				__func__, err);
		goto err_free_irq;
	}

	err =
		sysfs_create_group(&acceld->input_dev->dev.kobj,
						   &kionix_accel_attribute_group);

	if (err) {
		KMSGERR(&acceld->client->dev,
				"%s: sysfs_create_group returned err = %d. Abort.\n",
				__func__, err);
		goto err_free_irq;
	}

	kobject_uevent(&acceld->input_dev->dev.kobj, KOBJ_ADD);
	err = misc_register(&kionix_acc_misc_device);

	if (err < 0) {
		dev_err(&client->dev,
				"misc KIONIX_ACC_DEV_NAME register failed\n");
		goto err_free_sysfs;
	}

#if 0				/*def	 CONFIG_HAS_EARLYSUSPEND */
	/* The higher the level, the earlier it resume, and the later it suspend */
	acceld->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 50;
	acceld->early_suspend.suspend = kionix_accel_earlysuspend_suspend;
	acceld->early_suspend.resume = kionix_accel_earlysuspend_resume;
	register_early_suspend(&acceld->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */
	KMSGINF(&client->dev, "<%s> probe successfull L%d.\n", __func__,
			__LINE__);
	g_sensor_proc_entry =
		proc_create(G_SENSOR_CALI_PROCNAME, S_IRUGO, NULL,
					&g_sensor_cali_ops);

	if (NULL == g_sensor_proc_entry) {
		printk("kionix_accel_init can't do proc_create");
	}

#if defined(CONFIG_HQ_HARDWAREINFO)
	hq_get_gsensor_name("KXTJ2-1009");
#endif
	return 0;
err_free_sysfs:
	sysfs_remove_group(&client->dev.kobj, &kionix_accel_attribute_group);
err_free_irq:

	if (acceld->accel_drdy)
		free_irq(client->irq, acceld);

	destroy_workqueue(acceld->accel_workqueue);
	input_unregister_device(acceld->input_dev);
err_free_accel_registers:
	kfree(acceld->accel_registers);
err_accel_pdata_exit:

	if (accel_pdata->exit)
		accel_pdata->exit();

err_accel_pdata_power_off:
	kionix_accel_power_off(acceld);
err_free_mem:
	kfree(acceld);
err_malloc_acceld:
err_i2c_check_func:
err_gpio_request_failed:
	kfree(accel_pdata);
	KMSGERR(&client->dev, "<%s> probe fail L%d.\n", __func__, __LINE__);
	return err;
}

static int kionix_accel_remove(struct i2c_client *client)
{
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);
	remove_proc_entry(G_SENSOR_CALI_PROCNAME, NULL);
	misc_deregister(&kionix_acc_misc_device);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&acceld->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */
	sysfs_remove_group(&client->dev.kobj, &kionix_accel_attribute_group);

	if (acceld->accel_drdy)
		free_irq(client->irq, acceld);

	destroy_workqueue(acceld->accel_workqueue);
	input_unregister_device(acceld->input_dev);
	kfree(acceld->accel_registers);

	if (acceld->accel_pdata.exit)
		acceld->accel_pdata.exit();

	kionix_accel_power_off(acceld);
	kfree(acceld);
	return 0;
}

static int kionix_kxtj1057_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	KMSGINF(&acceld->client->dev,
				"kionix_kxtj1057_pm_suspend\n");
	acceld->on_before_suspend = atomic_read(&acceld->accel_enabled);
	if (acceld->on_before_suspend) {
		kionix_accel_disable(acceld);
		atomic_set(&acceld->accel_enabled, 0);
	}
	return 0;
}

static int kionix_kxtj1057_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kionix_accel_driver *acceld = i2c_get_clientdata(client);

	KMSGINF(&acceld->client->dev,
				"kionix_kxtj1057_pm_resume\n");
	if (acceld->on_before_suspend) {
		kionix_accel_enable(acceld);
		atomic_set(&acceld->accel_enabled, 1);
	}
    return 0;
}

static const struct dev_pm_ops KIONIX_PM_OPS = {
    .suspend = kionix_kxtj1057_pm_suspend,
    .resume = kionix_kxtj1057_pm_resume,
};


static const struct i2c_device_id kionix_accel_id[] = {
	{KIONIX_ACCEL_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, kionix_accel_id);
#ifdef CONFIG_OF
static const struct of_device_id kionix_accel_of_match[] = {
	{.compatible = "Kionix,accel",},
	{}
};

MODULE_DEVICE_TABLE(of, kionix_accel_of_match);
#endif /*CONFIG_OF */
static struct i2c_driver kionix_accel_driver = {
	.driver = {
		.name = KIONIX_ACCEL_NAME,
		.owner = THIS_MODULE,
		.pm = &KIONIX_PM_OPS,
#ifdef CONFIG_OF
		.of_match_table = kionix_accel_of_match,
#endif /*CONFIG_OF */
	},
	.probe = kionix_accel_probe,
	.remove = kionix_accel_remove,
	.id_table = kionix_accel_id,
};

static int __init kionix_accel_init(void)
{
	return i2c_add_driver(&kionix_accel_driver);
}

module_init(kionix_accel_init);
static void __exit kionix_accel_exit(void)
{
	i2c_del_driver(&kionix_accel_driver);
}

module_exit(kionix_accel_exit);
MODULE_DESCRIPTION("Kionix accelerometer driver");
MODULE_AUTHOR("Kuching Tan <kuchingtan@kionix.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("3.3.0");
