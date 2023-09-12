/*
 *  stk8baxx.h - Definitions for sensortek stk8baxx accelerometer
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
 *
 */

#ifndef __STK8BAXX__
#define __STK8BAXX__

#include <linux/ioctl.h>
#include <linux/types.h>

/* IOCTLs*/
#define STKDIR						77
#define STK_IOCTL_WRITE				_IOW(STKDIR, 0x05, char[8])
#define STK_IOCTL_READ				_IOWR(STKDIR, 0x06, char[8])
#define STK_IOCTL_SET_ENABLE			_IOW(STKDIR, 0x02, int)
#define STK_IOCTL_GET_ENABLE			_IOR(STKDIR, 0x03, int)
#define STK_IOCTL_SET_DELAY			_IOW(STKDIR, 0x00, int)
#define STK_IOCTL_GET_DELAY			_IOR(STKDIR, 0x01, int)
#define STK_IOCTL_SET_OFFSET			_IOW(STKDIR, 0x07, int[3])
#define STK_IOCTL_GET_OFFSET			_IOR(STKDIR, 0x08, int[3])
#define STK_IOCTL_GET_ACCELERATION	_IOR(STKDIR, 0x09, int[3])
#define STK_IOCTL_SET_RANGE			_IOW(STKDIR, 0x10, char)
#define STK_IOCTL_GET_RANGE			_IOR(STKDIR, 0x11, char)
#define STK_IOCTL_SET_CALI			_IOW(STKDIR, 0x12, char)

#define STK_IOCTL_GET_COOR_XYZ           _IOW(STKDIR, 22, int)
#define STK_IOCTL_GET_CHIP_ID            _IOR(STKDIR, 255, char[32])

struct stk8baxx_platform_data {
	unsigned char direction;
	int interrupt_pin;
};



#endif	/* #ifndef __STK8BAXX__ */
