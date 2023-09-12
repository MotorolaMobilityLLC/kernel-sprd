
/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
*
* File Name          : n2dm_misc.h
* Authors            : MH - C&I BU - Application Team
*		     : Carmine Iascone (carmine.iascone@st.com)
*		     : Matteo Dameno (matteo.dameno@st.com)
* Version            : V 1.0.5
* Date               : 26/08/2010
*
********************************************************************************
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
*******************************************************************************/
/*******************************************************************************
* Version History.
*
* Revision 1-0-0 05/11/2009
* First Release
* Revision 1-0-1 26/01/2010
* Linux K&R Compliant Release
* Revision 1-0-5 16/08/2010
* Interrupt Management
*
*******************************************************************************/

#ifndef	__N2DM_H__
#define	__N2DM_H__

#include	<linux/ioctl.h>	/* For IOCTL macros */
#include	<linux/input.h>

#ifndef DEBUG
#define DEBUG
#endif

#define SAD0L			0x00
#define SAD0H			0x01
#define N2DM_ACC_I2C_SADROOT	0x0C
#define N2DM_ACC_I2C_SAD_L	((N2DM_ACC_I2C_SADROOT<<1)|SAD0L)
#define N2DM_ACC_I2C_SAD_H	((N2DM_ACC_I2C_SADROOT<<1)|SAD0H)
#define	N2DM_ACC_DEV_NAME	"n2dm_acc"

/* SAO pad is connected to GND, set LSB of SAD '0' */
#define N2DM_ACC_I2C_ADDR     N2DM_ACC_I2C_SAD_L
#define N2DM_ACC_I2C_NAME     N2DM_ACC_DEV_NAME

#define	N2DM_ACC_IOCTL_BASE 77
/** The following define the IOCTL command values via the ioctl macros */
#define	N2DM_ACC_IOCTL_SET_DELAY	_IOW(N2DM_ACC_IOCTL_BASE, 0, int)
#define	N2DM_ACC_IOCTL_GET_DELAY	_IOR(N2DM_ACC_IOCTL_BASE, 1, int)
#define	N2DM_ACC_IOCTL_SET_ENABLE	_IOW(N2DM_ACC_IOCTL_BASE, 2, int)
#define	N2DM_ACC_IOCTL_GET_ENABLE	_IOR(N2DM_ACC_IOCTL_BASE, 3, int)
#define	N2DM_ACC_IOCTL_SET_FULLSCALE	_IOW(N2DM_ACC_IOCTL_BASE, 4, int)
#define	N2DM_ACC_IOCTL_SET_G_RANGE	N2DM_ACC_IOCTL_SET_FULLSCALE

#define	N2DM_ACC_IOCTL_SET_CTRL_REG3	_IOW(N2DM_ACC_IOCTL_BASE, 6, int)
#define	N2DM_ACC_IOCTL_SET_CTRL_REG6	_IOW(N2DM_ACC_IOCTL_BASE, 7, int)
#define	N2DM_ACC_IOCTL_SET_DURATION1	_IOW(N2DM_ACC_IOCTL_BASE, 8, int)
#define	N2DM_ACC_IOCTL_SET_THRESHOLD1	_IOW(N2DM_ACC_IOCTL_BASE, 9, int)
#define	N2DM_ACC_IOCTL_SET_CONFIG1	_IOW(N2DM_ACC_IOCTL_BASE, 10, int)

#define	N2DM_ACC_IOCTL_SET_DURATION2	_IOW(N2DM_ACC_IOCTL_BASE, 11, int)
#define	N2DM_ACC_IOCTL_SET_THRESHOLD2	_IOW(N2DM_ACC_IOCTL_BASE, 12, int)
#define	N2DM_ACC_IOCTL_SET_CONFIG2	_IOW(N2DM_ACC_IOCTL_BASE, 13, int)

#define	N2DM_ACC_IOCTL_GET_SOURCE1	_IOW(N2DM_ACC_IOCTL_BASE, 14, int)
#define	N2DM_ACC_IOCTL_GET_SOURCE2	_IOW(N2DM_ACC_IOCTL_BASE, 15, int)

#define	N2DM_ACC_IOCTL_GET_TAP_SOURCE	_IOW(N2DM_ACC_IOCTL_BASE, 16, int)

#define	N2DM_ACC_IOCTL_SET_TAP_TW	_IOW(N2DM_ACC_IOCTL_BASE, 17, int)
#define	N2DM_ACC_IOCTL_SET_TAP_CFG	_IOW(N2DM_ACC_IOCTL_BASE, 18, int)
#define	N2DM_ACC_IOCTL_SET_TAP_TLIM	_IOW(N2DM_ACC_IOCTL_BASE, 19, int)
#define	N2DM_ACC_IOCTL_SET_TAP_THS	_IOW(N2DM_ACC_IOCTL_BASE, 20, int)
#define	N2DM_ACC_IOCTL_SET_TAP_TLAT	_IOW(N2DM_ACC_IOCTL_BASE, 21, int)

#define N2DM_ACC_IOCTL_GET_COOR_XYZ	_IOW(N2DM_ACC_IOCTL_BASE, 22, int)

#define N2DM_ACC_IOCTL_GET_CHIP_ID	_IOR(N2DM_ACC_IOCTL_BASE, 255, char[32])

/************************************************/
/*      Accelerometer defines section           */
/************************************************/

/* Accelerometer Sensor Full Scale */
#define	N2DM_ACC_FS_MASK	0x30
#define N2DM_ACC_G_2G		0x00
#define N2DM_ACC_G_4G		0x10
#define N2DM_ACC_G_8G		0x20
#define N2DM_ACC_G_16G		0x30


/* Accelerometer Sensor Operating Mode */
#define N2DM_ACC_ENABLE		0x01
#define N2DM_ACC_DISABLE	0x00

struct n2dm_acc_platform_data {
	int poll_interval;
	int min_interval;

	int g_range;

	int axis_map_x;
	int axis_map_y;
	int axis_map_z;

	int negate_x;
	int negate_y;
	int negate_z;

	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);

	int gpio_int1;
	int gpio_int2;

};

#define MODNAME				"[N2DM]"
#undef pr_fmt
#define pr_fmt(fmt)  MODNAME"%s %d:" fmt, __func__, __LINE__
#define N2DM_FUN()        pr_info("\n")
#define N2DM_ERR(fmt, ...)  pr_err(fmt, ##__VA_ARGS__)
#define N2DM_LOG(fmt, ...)  pr_info(fmt, ##__VA_ARGS__)
#define N2DM_DBG(fmt, ...)  pr_debug(fmt, ##__VA_ARGS__)

#endif	/* __N2DM_H__ */



