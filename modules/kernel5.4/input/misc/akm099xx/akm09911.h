/*
 * Copyright (C) 2010 AKM, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;
 *
 */

/*
 * Definitions for AKM099XX magnetic sensor chip.
 */
#ifndef __AKM099XX_H__
#define __AKM099XX_H__

#include <linux/ioctl.h>

#define AKM099XX_I2C_NAME		"AKM099XX"

#define AKM099XX_REG_DATA		0x10
#define AKM099XX_REG_PRODUCTID_1		0x00

#define CONVERT_M			25
#define CONVERT_M_DIV		8192
#define CONVERT_O			45
#define CONVERT_O_DIV		8192

/* sensitivity 512 count = 1 Guass = 100uT*/

#define AKM099XX_OFFSET_X		32768
#define AKM099XX_OFFSET_Y		32768
#define AKM099XX_OFFSET_Z		32768
#define AKM099XX_SENSITIVITY_X		1024
#define AKM099XX_SENSITIVITY_Y		1024
#define AKM099XX_SENSITIVITY_Z		1024

#define MSENSOR						   0x83
#define AKM099XX_IOC_SET_DELAY         _IOW(MSENSOR, 0x29, short)
#define MMC31XX_IOC_TM					_IO(MSENSOR, 0x18)
#define MMC31XX_IOC_SET					_IO(MSENSOR, 0x19)
#define MMC31XX_IOC_RM					_IO(MSENSOR, 0x25)
#define MMC31XX_IOC_RESET				_IO(MSENSOR, 0x1a)
#define MMC31XX_IOC_RRM					_IO(MSENSOR, 0x26)
#define MMC31XX_IOC_READ				_IOR(MSENSOR, 0x1b, int[3])
#define MMC31XX_IOC_READXYZ				_IOR(MSENSOR, 0x1c, int[3])
#define ECOMPASS_IOC_GET_DELAY			_IOR(MSENSOR, 0x1d, int)
#define ECOMPASS_IOC_SET_YPR			_IOW(MSENSOR, 0x21, int[12])
#define ECOMPASS_IOC_GET_OPEN_STATUS	_IOR(MSENSOR, 0x20, int)
#define ECOMPASS_IOC_GET_MFLAG			_IOR(MSENSOR, 0x1e, short)
#define	ECOMPASS_IOC_GET_OFLAG			_IOR(MSENSOR, 0x1f, short)
#define MSENSOR_IOCTL_READ_SENSORDATA	_IOR(MSENSOR, 0x03, int)
#define ECOMPASS_IOC_GET_LAYOUT			_IOR(MSENSOR, 0X22, int)
#define AKM099XX_IOC_READ_REG		    _IOWR(MSENSOR, 0x23, unsigned char)
#define AKM099XX_IOC_WRITE_REG		    _IOW(MSENSOR,  0x24, unsigned char[2])
#define AKM099XX_IOC_READ_REGS		    _IOWR(MSENSOR, 0x25, unsigned char[10])
#define MSENSOR_IOCTL_SENSOR_ENABLE         _IOW(MSENSOR, 0x51, int)
#define ECOMPASS_IOC_SET_ACC_DATA               _IOW(MSENSOR, 0x52, int[3])
#define ECOMPASS_IOC_GET_ACC_DATA               _IOR(MSENSOR, 0x53, int[3])
#define MSENSOR_IOCTL_READ_FACTORY_SENSORDATA  _IOW(MSENSOR, 0x54, int)
#define MSENSOR_IOCTL_MSENSOR_ENABLE             _IOW(MSENSOR, 0x55, int)
#define MSENSOR_IOCTL_OSENSOR_ENABLE             _IOW(MSENSOR, 0x56, int)

#define AK099XX_SOFT_RESET               0x01

#define AK099XX_REG_WIA1                 0x00
#define AK099XX_REG_WIA2                 0x01

#define AK099XX_REG_CNTL2                0x31
#define AK099XX_REG_CNTL3                0x32
#define AK099XX_MODE_POWER_DOWN          0x00

#define AK099XX_MODE_SNG_MEASURE         0x01
#define AK099XX_MODE_CONT_MEASURE_MODE1  0x02
#define AK099XX_MODE_CONT_MEASURE_MODE2  0x04
#define AK099XX_MODE_CONT_MEASURE_MODE3  0x06
#define AK099XX_MODE_CONT_MEASURE_MODE4  0x08
#define AK099XX_MODE_CONT_MEASURE_MODE5  0x0A
#define AK099XX_MODE_CONT_MEASURE_MODE6  0x0C

#define AK099XX_MODE_FUSE_ACCESS         0x1F
#define AK099XX_FUSE_ASAX                0x60
#define AK099XX_FUSE_ASAY                0x61
#define AK099XX_FUSE_ASAZ                0x62

#endif /* __AKM099XX_H__ */
