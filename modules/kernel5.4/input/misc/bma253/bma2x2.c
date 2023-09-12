/*
 * File:bma2x2.c
 * Author:bao.yue@spreadtrum.com
 * Created:2017-03-15
 * Description:
 * This file contains all function implementations for the BMA2X2 in linux
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 */
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/workqueue.h>
#include <uapi/linux/sched/types.h>
#include "bstclass.h"

#define DRIVER_VERSION "0.0.23.1"
#define ACC_NAME  "acc"
#define BMA2X2_ENABLE_INT1 1
#define BMA_ENABLE_NEWDATA_INT 1

#define SENSOR_NAME                 "bma2x2"
#define MSC_TIME                6
#define G_MAX		16000	/* Maximum polled-device-reported g value */
#define SLOPE_THRESHOLD_VALUE       32
#define SLOPE_DURATION_VALUE        1
#define INTERRUPT_LATCH_MODE        13
#define INTERRUPT_ENABLE            1
#define INTERRUPT_DISABLE           0
#define MAP_SLOPE_INTERRUPT         2
#define SLOPE_X_INDEX               5
#define SLOPE_Y_INDEX               6
#define SLOPE_Z_INDEX               7
#define BMA2X2_MAX_DELAY           200
#define BMA2X2_RANGE_SET_DEFAULT          BMA2X2_RANGE_4G /* +/- 4G */
#define BMA2X2_BW_SET_DEFAULT               BMA2X2_BW_62_50HZ /* 62HZ  */

#define LOW_G_INTERRUPT             REL_Z
#define HIGH_G_INTERRUPT            REL_HWHEEL
#define SLOP_INTERRUPT              REL_DIAL
#define DOUBLE_TAP_INTERRUPT        REL_WHEEL
#define SINGLE_TAP_INTERRUPT        REL_MISC
#define ORIENT_INTERRUPT            ABS_PRESSURE
#define FLAT_INTERRUPT              ABS_DISTANCE
#define SLOW_NO_MOTION_INTERRUPT    REL_Y

#define HIGH_G_INTERRUPT_X_HAPPENED                 1
#define HIGH_G_INTERRUPT_Y_HAPPENED                 2
#define HIGH_G_INTERRUPT_Z_HAPPENED                 3
#define HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED        4
#define HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED        5
#define HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED        6
#define SLOPE_INTERRUPT_X_HAPPENED                  7
#define SLOPE_INTERRUPT_Y_HAPPENED                  8
#define SLOPE_INTERRUPT_Z_HAPPENED                  9
#define SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED         10
#define SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED         11
#define SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED         12
#define DOUBLE_TAP_INTERRUPT_HAPPENED               13
#define SINGLE_TAP_INTERRUPT_HAPPENED               14
#define UPWARD_PORTRAIT_UP_INTERRUPT_HAPPENED       15
#define UPWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED     16
#define UPWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED    17
#define UPWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED   18
#define DOWNWARD_PORTRAIT_UP_INTERRUPT_HAPPENED     19
#define DOWNWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED   20
#define DOWNWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED  21
#define DOWNWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED 22
#define FLAT_INTERRUPT_TRUE_HAPPENED                23
#define FLAT_INTERRUPT_FALSE_HAPPENED               24
#define LOW_G_INTERRUPT_HAPPENED                    25
#define SLOW_NO_MOTION_INTERRUPT_HAPPENED           26

#define PAD_LOWG                    0
#define PAD_HIGHG                   1
#define PAD_SLOP                    2
#define PAD_DOUBLE_TAP              3
#define PAD_SINGLE_TAP              4
#define PAD_ORIENT                  5
#define PAD_FLAT                    6
#define PAD_SLOW_NO_MOTION          7

#define BMA2X2_EEP_OFFSET                       0x16
#define BMA2X2_IMAGE_BASE                       0x38
#define BMA2X2_IMAGE_LEN                        22

#define BMA2X2_CHIP_ID_REG                      0x00
#define BMA2X2_VERSION_REG                      0x01
#define BMA2X2_X_AXIS_LSB_REG                   0x02
#define BMA2X2_X_AXIS_MSB_REG                   0x03
#define BMA2X2_Y_AXIS_LSB_REG                   0x04
#define BMA2X2_Y_AXIS_MSB_REG                   0x05
#define BMA2X2_Z_AXIS_LSB_REG                   0x06
#define BMA2X2_Z_AXIS_MSB_REG                   0x07
#define BMA2X2_TEMPERATURE_REG                  0x08
#define BMA2X2_STATUS1_REG                      0x09
#define BMA2X2_STATUS2_REG                      0x0A
#define BMA2X2_STATUS_TAP_SLOPE_REG             0x0B
#define BMA2X2_STATUS_ORIENT_HIGH_REG           0x0C
#define BMA2X2_STATUS_FIFO_REG                  0x0E
#define BMA2X2_RANGE_SEL_REG                    0x0F
#define BMA2X2_BW_SEL_REG                       0x10
#define BMA2X2_MODE_CTRL_REG                    0x11
#define BMA2X2_LOW_NOISE_CTRL_REG               0x12
#define BMA2X2_DATA_CTRL_REG                    0x13
#define BMA2X2_RESET_REG                        0x14
#define BMA2X2_INT_ENABLE1_REG                  0x16
#define BMA2X2_INT_ENABLE2_REG                  0x17
#define BMA2X2_INT_SLO_NO_MOT_REG               0x18
#define BMA2X2_INT1_PAD_SEL_REG                 0x19
#define BMA2X2_INT_DATA_SEL_REG                 0x1A
#define BMA2X2_INT2_PAD_SEL_REG                 0x1B
#define BMA2X2_INT_SRC_REG                      0x1E
#define BMA2X2_INT_SET_REG                      0x20
#define BMA2X2_INT_CTRL_REG                     0x21
#define BMA2X2_LOW_DURN_REG                     0x22
#define BMA2X2_LOW_THRES_REG                    0x23
#define BMA2X2_LOW_HIGH_HYST_REG                0x24
#define BMA2X2_HIGH_DURN_REG                    0x25
#define BMA2X2_HIGH_THRES_REG                   0x26
#define BMA2X2_SLOPE_DURN_REG                   0x27
#define BMA2X2_SLOPE_THRES_REG                  0x28
#define BMA2X2_SLO_NO_MOT_THRES_REG             0x29
#define BMA2X2_TAP_PARAM_REG                    0x2A
#define BMA2X2_TAP_THRES_REG                    0x2B
#define BMA2X2_ORIENT_PARAM_REG                 0x2C
#define BMA2X2_THETA_BLOCK_REG                  0x2D
#define BMA2X2_THETA_FLAT_REG                   0x2E
#define BMA2X2_FLAT_HOLD_TIME_REG               0x2F
#define BMA2X2_FIFO_WML_TRIG                    0x30
#define BMA2X2_SELF_TEST_REG                    0x32
#define BMA2X2_EEPROM_CTRL_REG                  0x33
#define BMA2X2_SERIAL_CTRL_REG                  0x34
#define BMA2X2_EXTMODE_CTRL_REG                 0x35
#define BMA2X2_OFFSET_CTRL_REG                  0x36
#define BMA2X2_OFFSET_PARAMS_REG                0x37
#define BMA2X2_OFFSET_X_AXIS_REG                0x38
#define BMA2X2_OFFSET_Y_AXIS_REG                0x39
#define BMA2X2_OFFSET_Z_AXIS_REG                0x3A
#define BMA2X2_GP0_REG                          0x3B
#define BMA2X2_GP1_REG                          0x3C
#define BMA2X2_FIFO_MODE_REG                    0x3E
#define BMA2X2_FIFO_DATA_OUTPUT_REG             0x3F

#define BMA2X2_CHIP_ID__POS             0
#define BMA2X2_CHIP_ID__MSK             0xFF
#define BMA2X2_CHIP_ID__LEN             8
#define BMA2X2_CHIP_ID__REG             BMA2X2_CHIP_ID_REG

#define BMA2X2_VERSION__POS          0
#define BMA2X2_VERSION__LEN          8
#define BMA2X2_VERSION__MSK          0xFF
#define BMA2X2_VERSION__REG          BMA2X2_VERSION_REG

#define BMA2x2_SLO_NO_MOT_DUR__POS   2
#define BMA2x2_SLO_NO_MOT_DUR__LEN   6
#define BMA2x2_SLO_NO_MOT_DUR__MSK   0xFC
#define BMA2x2_SLO_NO_MOT_DUR__REG   BMA2X2_SLOPE_DURN_REG

#define BMA2X2_NEW_DATA_X__POS          0
#define BMA2X2_NEW_DATA_X__LEN          1
#define BMA2X2_NEW_DATA_X__MSK          0x01
#define BMA2X2_NEW_DATA_X__REG          BMA2X2_X_AXIS_LSB_REG

#define BMA2X2_ACC_X14_LSB__POS           2
#define BMA2X2_ACC_X14_LSB__LEN           6
#define BMA2X2_ACC_X14_LSB__MSK           0xFC
#define BMA2X2_ACC_X14_LSB__REG           BMA2X2_X_AXIS_LSB_REG

#define BMA2X2_ACC_X12_LSB__POS           4
#define BMA2X2_ACC_X12_LSB__LEN           4
#define BMA2X2_ACC_X12_LSB__MSK           0xF0
#define BMA2X2_ACC_X12_LSB__REG           BMA2X2_X_AXIS_LSB_REG

#define BMA2X2_ACC_X10_LSB__POS           6
#define BMA2X2_ACC_X10_LSB__LEN           2
#define BMA2X2_ACC_X10_LSB__MSK           0xC0
#define BMA2X2_ACC_X10_LSB__REG           BMA2X2_X_AXIS_LSB_REG

#define BMA2X2_ACC_X8_LSB__POS           0
#define BMA2X2_ACC_X8_LSB__LEN           0
#define BMA2X2_ACC_X8_LSB__MSK           0x00
#define BMA2X2_ACC_X8_LSB__REG           BMA2X2_X_AXIS_LSB_REG

#define BMA2X2_ACC_X_MSB__POS           0
#define BMA2X2_ACC_X_MSB__LEN           8
#define BMA2X2_ACC_X_MSB__MSK           0xFF
#define BMA2X2_ACC_X_MSB__REG           BMA2X2_X_AXIS_MSB_REG

#define BMA2X2_NEW_DATA_Y__POS          0
#define BMA2X2_NEW_DATA_Y__LEN          1
#define BMA2X2_NEW_DATA_Y__MSK          0x01
#define BMA2X2_NEW_DATA_Y__REG          BMA2X2_Y_AXIS_LSB_REG

#define BMA2X2_ACC_Y14_LSB__POS           2
#define BMA2X2_ACC_Y14_LSB__LEN           6
#define BMA2X2_ACC_Y14_LSB__MSK           0xFC
#define BMA2X2_ACC_Y14_LSB__REG           BMA2X2_Y_AXIS_LSB_REG

#define BMA2X2_ACC_Y12_LSB__POS           4
#define BMA2X2_ACC_Y12_LSB__LEN           4
#define BMA2X2_ACC_Y12_LSB__MSK           0xF0
#define BMA2X2_ACC_Y12_LSB__REG           BMA2X2_Y_AXIS_LSB_REG

#define BMA2X2_ACC_Y10_LSB__POS           6
#define BMA2X2_ACC_Y10_LSB__LEN           2
#define BMA2X2_ACC_Y10_LSB__MSK           0xC0
#define BMA2X2_ACC_Y10_LSB__REG           BMA2X2_Y_AXIS_LSB_REG

#define BMA2X2_ACC_Y8_LSB__POS           0
#define BMA2X2_ACC_Y8_LSB__LEN           0
#define BMA2X2_ACC_Y8_LSB__MSK           0x00
#define BMA2X2_ACC_Y8_LSB__REG           BMA2X2_Y_AXIS_LSB_REG

#define BMA2X2_ACC_Y_MSB__POS           0
#define BMA2X2_ACC_Y_MSB__LEN           8
#define BMA2X2_ACC_Y_MSB__MSK           0xFF
#define BMA2X2_ACC_Y_MSB__REG           BMA2X2_Y_AXIS_MSB_REG

#define BMA2X2_NEW_DATA_Z__POS          0
#define BMA2X2_NEW_DATA_Z__LEN          1
#define BMA2X2_NEW_DATA_Z__MSK          0x01
#define BMA2X2_NEW_DATA_Z__REG          BMA2X2_Z_AXIS_LSB_REG

#define BMA2X2_ACC_Z14_LSB__POS           2
#define BMA2X2_ACC_Z14_LSB__LEN           6
#define BMA2X2_ACC_Z14_LSB__MSK           0xFC
#define BMA2X2_ACC_Z14_LSB__REG           BMA2X2_Z_AXIS_LSB_REG

#define BMA2X2_ACC_Z12_LSB__POS           4
#define BMA2X2_ACC_Z12_LSB__LEN           4
#define BMA2X2_ACC_Z12_LSB__MSK           0xF0
#define BMA2X2_ACC_Z12_LSB__REG           BMA2X2_Z_AXIS_LSB_REG

#define BMA2X2_ACC_Z10_LSB__POS           6
#define BMA2X2_ACC_Z10_LSB__LEN           2
#define BMA2X2_ACC_Z10_LSB__MSK           0xC0
#define BMA2X2_ACC_Z10_LSB__REG           BMA2X2_Z_AXIS_LSB_REG

#define BMA2X2_ACC_Z8_LSB__POS           0
#define BMA2X2_ACC_Z8_LSB__LEN           0
#define BMA2X2_ACC_Z8_LSB__MSK           0x00
#define BMA2X2_ACC_Z8_LSB__REG           BMA2X2_Z_AXIS_LSB_REG

#define BMA2X2_ACC_Z_MSB__POS           0
#define BMA2X2_ACC_Z_MSB__LEN           8
#define BMA2X2_ACC_Z_MSB__MSK           0xFF
#define BMA2X2_ACC_Z_MSB__REG           BMA2X2_Z_AXIS_MSB_REG

#define BMA2X2_TEMPERATURE__POS         0
#define BMA2X2_TEMPERATURE__LEN         8
#define BMA2X2_TEMPERATURE__MSK         0xFF
#define BMA2X2_TEMPERATURE__REG         BMA2X2_TEMP_RD_REG

#define BMA2X2_LOWG_INT_S__POS          0
#define BMA2X2_LOWG_INT_S__LEN          1
#define BMA2X2_LOWG_INT_S__MSK          0x01
#define BMA2X2_LOWG_INT_S__REG          BMA2X2_STATUS1_REG

#define BMA2X2_HIGHG_INT_S__POS          1
#define BMA2X2_HIGHG_INT_S__LEN          1
#define BMA2X2_HIGHG_INT_S__MSK          0x02
#define BMA2X2_HIGHG_INT_S__REG          BMA2X2_STATUS1_REG

#define BMA2X2_SLOPE_INT_S__POS          2
#define BMA2X2_SLOPE_INT_S__LEN          1
#define BMA2X2_SLOPE_INT_S__MSK          0x04
#define BMA2X2_SLOPE_INT_S__REG          BMA2X2_STATUS1_REG


#define BMA2X2_SLO_NO_MOT_INT_S__POS          3
#define BMA2X2_SLO_NO_MOT_INT_S__LEN          1
#define BMA2X2_SLO_NO_MOT_INT_S__MSK          0x08
#define BMA2X2_SLO_NO_MOT_INT_S__REG          BMA2X2_STATUS1_REG

#define BMA2X2_DOUBLE_TAP_INT_S__POS     4
#define BMA2X2_DOUBLE_TAP_INT_S__LEN     1
#define BMA2X2_DOUBLE_TAP_INT_S__MSK     0x10
#define BMA2X2_DOUBLE_TAP_INT_S__REG     BMA2X2_STATUS1_REG

#define BMA2X2_SINGLE_TAP_INT_S__POS     5
#define BMA2X2_SINGLE_TAP_INT_S__LEN     1
#define BMA2X2_SINGLE_TAP_INT_S__MSK     0x20
#define BMA2X2_SINGLE_TAP_INT_S__REG     BMA2X2_STATUS1_REG

#define BMA2X2_ORIENT_INT_S__POS         6
#define BMA2X2_ORIENT_INT_S__LEN         1
#define BMA2X2_ORIENT_INT_S__MSK         0x40
#define BMA2X2_ORIENT_INT_S__REG         BMA2X2_STATUS1_REG

#define BMA2X2_FLAT_INT_S__POS           7
#define BMA2X2_FLAT_INT_S__LEN           1
#define BMA2X2_FLAT_INT_S__MSK           0x80
#define BMA2X2_FLAT_INT_S__REG           BMA2X2_STATUS1_REG

#define BMA2X2_FIFO_FULL_INT_S__POS           5
#define BMA2X2_FIFO_FULL_INT_S__LEN           1
#define BMA2X2_FIFO_FULL_INT_S__MSK           0x20
#define BMA2X2_FIFO_FULL_INT_S__REG           BMA2X2_STATUS2_REG

#define BMA2X2_FIFO_WM_INT_S__POS           6
#define BMA2X2_FIFO_WM_INT_S__LEN           1
#define BMA2X2_FIFO_WM_INT_S__MSK           0x40
#define BMA2X2_FIFO_WM_INT_S__REG           BMA2X2_STATUS2_REG

#define BMA2X2_DATA_INT_S__POS           7
#define BMA2X2_DATA_INT_S__LEN           1
#define BMA2X2_DATA_INT_S__MSK           0x80
#define BMA2X2_DATA_INT_S__REG           BMA2X2_STATUS2_REG

#define BMA2X2_SLOPE_FIRST_X__POS        0
#define BMA2X2_SLOPE_FIRST_X__LEN        1
#define BMA2X2_SLOPE_FIRST_X__MSK        0x01
#define BMA2X2_SLOPE_FIRST_X__REG        BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_SLOPE_FIRST_Y__POS        1
#define BMA2X2_SLOPE_FIRST_Y__LEN        1
#define BMA2X2_SLOPE_FIRST_Y__MSK        0x02
#define BMA2X2_SLOPE_FIRST_Y__REG        BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_SLOPE_FIRST_Z__POS        2
#define BMA2X2_SLOPE_FIRST_Z__LEN        1
#define BMA2X2_SLOPE_FIRST_Z__MSK        0x04
#define BMA2X2_SLOPE_FIRST_Z__REG        BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_SLOPE_SIGN_S__POS         3
#define BMA2X2_SLOPE_SIGN_S__LEN         1
#define BMA2X2_SLOPE_SIGN_S__MSK         0x08
#define BMA2X2_SLOPE_SIGN_S__REG         BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_TAP_FIRST_X__POS        4
#define BMA2X2_TAP_FIRST_X__LEN        1
#define BMA2X2_TAP_FIRST_X__MSK        0x10
#define BMA2X2_TAP_FIRST_X__REG        BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_TAP_FIRST_Y__POS        5
#define BMA2X2_TAP_FIRST_Y__LEN        1
#define BMA2X2_TAP_FIRST_Y__MSK        0x20
#define BMA2X2_TAP_FIRST_Y__REG        BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_TAP_FIRST_Z__POS        6
#define BMA2X2_TAP_FIRST_Z__LEN        1
#define BMA2X2_TAP_FIRST_Z__MSK        0x40
#define BMA2X2_TAP_FIRST_Z__REG        BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_TAP_SIGN_S__POS         7
#define BMA2X2_TAP_SIGN_S__LEN         1
#define BMA2X2_TAP_SIGN_S__MSK         0x80
#define BMA2X2_TAP_SIGN_S__REG         BMA2X2_STATUS_TAP_SLOPE_REG

#define BMA2X2_HIGHG_FIRST_X__POS        0
#define BMA2X2_HIGHG_FIRST_X__LEN        1
#define BMA2X2_HIGHG_FIRST_X__MSK        0x01
#define BMA2X2_HIGHG_FIRST_X__REG        BMA2X2_STATUS_ORIENT_HIGH_REG

#define BMA2X2_HIGHG_FIRST_Y__POS        1
#define BMA2X2_HIGHG_FIRST_Y__LEN        1
#define BMA2X2_HIGHG_FIRST_Y__MSK        0x02
#define BMA2X2_HIGHG_FIRST_Y__REG        BMA2X2_STATUS_ORIENT_HIGH_REG

#define BMA2X2_HIGHG_FIRST_Z__POS        2
#define BMA2X2_HIGHG_FIRST_Z__LEN        1
#define BMA2X2_HIGHG_FIRST_Z__MSK        0x04
#define BMA2X2_HIGHG_FIRST_Z__REG        BMA2X2_STATUS_ORIENT_HIGH_REG

#define BMA2X2_HIGHG_SIGN_S__POS         3
#define BMA2X2_HIGHG_SIGN_S__LEN         1
#define BMA2X2_HIGHG_SIGN_S__MSK         0x08
#define BMA2X2_HIGHG_SIGN_S__REG         BMA2X2_STATUS_ORIENT_HIGH_REG

#define BMA2X2_ORIENT_S__POS             4
#define BMA2X2_ORIENT_S__LEN             3
#define BMA2X2_ORIENT_S__MSK             0x70
#define BMA2X2_ORIENT_S__REG             BMA2X2_STATUS_ORIENT_HIGH_REG

#define BMA2X2_FLAT_S__POS               7
#define BMA2X2_FLAT_S__LEN               1
#define BMA2X2_FLAT_S__MSK               0x80
#define BMA2X2_FLAT_S__REG               BMA2X2_STATUS_ORIENT_HIGH_REG

#define BMA2X2_FIFO_FRAME_COUNTER_S__POS             0
#define BMA2X2_FIFO_FRAME_COUNTER_S__LEN             7
#define BMA2X2_FIFO_FRAME_COUNTER_S__MSK             0x7F
#define BMA2X2_FIFO_FRAME_COUNTER_S__REG             BMA2X2_STATUS_FIFO_REG

#define BMA2X2_FIFO_OVERRUN_S__POS             7
#define BMA2X2_FIFO_OVERRUN_S__LEN             1
#define BMA2X2_FIFO_OVERRUN_S__MSK             0x80
#define BMA2X2_FIFO_OVERRUN_S__REG             BMA2X2_STATUS_FIFO_REG

#define BMA2X2_RANGE_SEL__POS             0
#define BMA2X2_RANGE_SEL__LEN             4
#define BMA2X2_RANGE_SEL__MSK             0x0F
#define BMA2X2_RANGE_SEL__REG             BMA2X2_RANGE_SEL_REG

#define BMA2X2_BANDWIDTH__POS             0
#define BMA2X2_BANDWIDTH__LEN             5
#define BMA2X2_BANDWIDTH__MSK             0x1F
#define BMA2X2_BANDWIDTH__REG             BMA2X2_BW_SEL_REG

#define BMA2X2_SLEEP_DUR__POS             1
#define BMA2X2_SLEEP_DUR__LEN             4
#define BMA2X2_SLEEP_DUR__MSK             0x1E
#define BMA2X2_SLEEP_DUR__REG             BMA2X2_MODE_CTRL_REG

#define BMA2X2_MODE_CTRL__POS             5
#define BMA2X2_MODE_CTRL__LEN             3
#define BMA2X2_MODE_CTRL__MSK             0xE0
#define BMA2X2_MODE_CTRL__REG             BMA2X2_MODE_CTRL_REG

#define BMA2X2_DEEP_SUSPEND__POS          5
#define BMA2X2_DEEP_SUSPEND__LEN          1
#define BMA2X2_DEEP_SUSPEND__MSK          0x20
#define BMA2X2_DEEP_SUSPEND__REG          BMA2X2_MODE_CTRL_REG

#define BMA2X2_EN_LOW_POWER__POS          6
#define BMA2X2_EN_LOW_POWER__LEN          1
#define BMA2X2_EN_LOW_POWER__MSK          0x40
#define BMA2X2_EN_LOW_POWER__REG          BMA2X2_MODE_CTRL_REG

#define BMA2X2_EN_SUSPEND__POS            7
#define BMA2X2_EN_SUSPEND__LEN            1
#define BMA2X2_EN_SUSPEND__MSK            0x80
#define BMA2X2_EN_SUSPEND__REG            BMA2X2_MODE_CTRL_REG

#define BMA2X2_SLEEP_TIMER__POS          5
#define BMA2X2_SLEEP_TIMER__LEN          1
#define BMA2X2_SLEEP_TIMER__MSK          0x20
#define BMA2X2_SLEEP_TIMER__REG          BMA2X2_LOW_NOISE_CTRL_REG

#define BMA2X2_LOW_POWER_MODE__POS          6
#define BMA2X2_LOW_POWER_MODE__LEN          1
#define BMA2X2_LOW_POWER_MODE__MSK          0x40
#define BMA2X2_LOW_POWER_MODE__REG          BMA2X2_LOW_NOISE_CTRL_REG

#define BMA2X2_EN_LOW_NOISE__POS          7
#define BMA2X2_EN_LOW_NOISE__LEN          1
#define BMA2X2_EN_LOW_NOISE__MSK          0x80
#define BMA2X2_EN_LOW_NOISE__REG          BMA2X2_LOW_NOISE_CTRL_REG

#define BMA2X2_DIS_SHADOW_PROC__POS       6
#define BMA2X2_DIS_SHADOW_PROC__LEN       1
#define BMA2X2_DIS_SHADOW_PROC__MSK       0x40
#define BMA2X2_DIS_SHADOW_PROC__REG       BMA2X2_DATA_CTRL_REG

#define BMA2X2_EN_DATA_HIGH_BW__POS         7
#define BMA2X2_EN_DATA_HIGH_BW__LEN         1
#define BMA2X2_EN_DATA_HIGH_BW__MSK         0x80
#define BMA2X2_EN_DATA_HIGH_BW__REG         BMA2X2_DATA_CTRL_REG

#define BMA2X2_EN_SOFT_RESET__POS         0
#define BMA2X2_EN_SOFT_RESET__LEN         8
#define BMA2X2_EN_SOFT_RESET__MSK         0xFF
#define BMA2X2_EN_SOFT_RESET__REG         BMA2X2_RESET_REG

#define BMA2X2_EN_SOFT_RESET_VALUE        0xB6

#define BMA2X2_EN_SLOPE_X_INT__POS         0
#define BMA2X2_EN_SLOPE_X_INT__LEN         1
#define BMA2X2_EN_SLOPE_X_INT__MSK         0x01
#define BMA2X2_EN_SLOPE_X_INT__REG         BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_SLOPE_Y_INT__POS         1
#define BMA2X2_EN_SLOPE_Y_INT__LEN         1
#define BMA2X2_EN_SLOPE_Y_INT__MSK         0x02
#define BMA2X2_EN_SLOPE_Y_INT__REG         BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_SLOPE_Z_INT__POS         2
#define BMA2X2_EN_SLOPE_Z_INT__LEN         1
#define BMA2X2_EN_SLOPE_Z_INT__MSK         0x04
#define BMA2X2_EN_SLOPE_Z_INT__REG         BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_DOUBLE_TAP_INT__POS      4
#define BMA2X2_EN_DOUBLE_TAP_INT__LEN      1
#define BMA2X2_EN_DOUBLE_TAP_INT__MSK      0x10
#define BMA2X2_EN_DOUBLE_TAP_INT__REG      BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_SINGLE_TAP_INT__POS      5
#define BMA2X2_EN_SINGLE_TAP_INT__LEN      1
#define BMA2X2_EN_SINGLE_TAP_INT__MSK      0x20
#define BMA2X2_EN_SINGLE_TAP_INT__REG      BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_ORIENT_INT__POS          6
#define BMA2X2_EN_ORIENT_INT__LEN          1
#define BMA2X2_EN_ORIENT_INT__MSK          0x40
#define BMA2X2_EN_ORIENT_INT__REG          BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_FLAT_INT__POS            7
#define BMA2X2_EN_FLAT_INT__LEN            1
#define BMA2X2_EN_FLAT_INT__MSK            0x80
#define BMA2X2_EN_FLAT_INT__REG            BMA2X2_INT_ENABLE1_REG

#define BMA2X2_EN_HIGHG_X_INT__POS         0
#define BMA2X2_EN_HIGHG_X_INT__LEN         1
#define BMA2X2_EN_HIGHG_X_INT__MSK         0x01
#define BMA2X2_EN_HIGHG_X_INT__REG         BMA2X2_INT_ENABLE2_REG

#define BMA2X2_EN_HIGHG_Y_INT__POS         1
#define BMA2X2_EN_HIGHG_Y_INT__LEN         1
#define BMA2X2_EN_HIGHG_Y_INT__MSK         0x02
#define BMA2X2_EN_HIGHG_Y_INT__REG         BMA2X2_INT_ENABLE2_REG

#define BMA2X2_EN_HIGHG_Z_INT__POS         2
#define BMA2X2_EN_HIGHG_Z_INT__LEN         1
#define BMA2X2_EN_HIGHG_Z_INT__MSK         0x04
#define BMA2X2_EN_HIGHG_Z_INT__REG         BMA2X2_INT_ENABLE2_REG

#define BMA2X2_EN_LOWG_INT__POS            3
#define BMA2X2_EN_LOWG_INT__LEN            1
#define BMA2X2_EN_LOWG_INT__MSK            0x08
#define BMA2X2_EN_LOWG_INT__REG            BMA2X2_INT_ENABLE2_REG

#define BMA2X2_EN_NEW_DATA_INT__POS        4
#define BMA2X2_EN_NEW_DATA_INT__LEN        1
#define BMA2X2_EN_NEW_DATA_INT__MSK        0x10
#define BMA2X2_EN_NEW_DATA_INT__REG        BMA2X2_INT_ENABLE2_REG

#define BMA2X2_INT_FFULL_EN_INT__POS        5
#define BMA2X2_INT_FFULL_EN_INT__LEN        1
#define BMA2X2_INT_FFULL_EN_INT__MSK        0x20
#define BMA2X2_INT_FFULL_EN_INT__REG        BMA2X2_INT_ENABLE2_REG

#define BMA2X2_INT_FWM_EN_INT__POS        6
#define BMA2X2_INT_FWM_EN_INT__LEN        1
#define BMA2X2_INT_FWM_EN_INT__MSK        0x40
#define BMA2X2_INT_FWM_EN_INT__REG        BMA2X2_INT_ENABLE2_REG

#define BMA2X2_INT_SLO_NO_MOT_EN_X_INT__POS        0
#define BMA2X2_INT_SLO_NO_MOT_EN_X_INT__LEN        1
#define BMA2X2_INT_SLO_NO_MOT_EN_X_INT__MSK        0x01
#define BMA2X2_INT_SLO_NO_MOT_EN_X_INT__REG        BMA2X2_INT_SLO_NO_MOT_REG

#define BMA2X2_INT_SLO_NO_MOT_EN_Y_INT__POS        1
#define BMA2X2_INT_SLO_NO_MOT_EN_Y_INT__LEN        1
#define BMA2X2_INT_SLO_NO_MOT_EN_Y_INT__MSK        0x02
#define BMA2X2_INT_SLO_NO_MOT_EN_Y_INT__REG        BMA2X2_INT_SLO_NO_MOT_REG

#define BMA2X2_INT_SLO_NO_MOT_EN_Z_INT__POS        2
#define BMA2X2_INT_SLO_NO_MOT_EN_Z_INT__LEN        1
#define BMA2X2_INT_SLO_NO_MOT_EN_Z_INT__MSK        0x04
#define BMA2X2_INT_SLO_NO_MOT_EN_Z_INT__REG        BMA2X2_INT_SLO_NO_MOT_REG

#define BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT__POS        3
#define BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT__LEN        1
#define BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT__MSK        0x08
#define BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT__REG        BMA2X2_INT_SLO_NO_MOT_REG

#define BMA2X2_EN_INT1_PAD_LOWG__POS        0
#define BMA2X2_EN_INT1_PAD_LOWG__LEN        1
#define BMA2X2_EN_INT1_PAD_LOWG__MSK        0x01
#define BMA2X2_EN_INT1_PAD_LOWG__REG        BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_HIGHG__POS       1
#define BMA2X2_EN_INT1_PAD_HIGHG__LEN       1
#define BMA2X2_EN_INT1_PAD_HIGHG__MSK       0x02
#define BMA2X2_EN_INT1_PAD_HIGHG__REG       BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_SLOPE__POS       2
#define BMA2X2_EN_INT1_PAD_SLOPE__LEN       1
#define BMA2X2_EN_INT1_PAD_SLOPE__MSK       0x04
#define BMA2X2_EN_INT1_PAD_SLOPE__REG       BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_SLO_NO_MOT__POS        3
#define BMA2X2_EN_INT1_PAD_SLO_NO_MOT__LEN        1
#define BMA2X2_EN_INT1_PAD_SLO_NO_MOT__MSK        0x08
#define BMA2X2_EN_INT1_PAD_SLO_NO_MOT__REG        BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_DB_TAP__POS      4
#define BMA2X2_EN_INT1_PAD_DB_TAP__LEN      1
#define BMA2X2_EN_INT1_PAD_DB_TAP__MSK      0x10
#define BMA2X2_EN_INT1_PAD_DB_TAP__REG      BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_SNG_TAP__POS     5
#define BMA2X2_EN_INT1_PAD_SNG_TAP__LEN     1
#define BMA2X2_EN_INT1_PAD_SNG_TAP__MSK     0x20
#define BMA2X2_EN_INT1_PAD_SNG_TAP__REG     BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_ORIENT__POS      6
#define BMA2X2_EN_INT1_PAD_ORIENT__LEN      1
#define BMA2X2_EN_INT1_PAD_ORIENT__MSK      0x40
#define BMA2X2_EN_INT1_PAD_ORIENT__REG      BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_FLAT__POS        7
#define BMA2X2_EN_INT1_PAD_FLAT__LEN        1
#define BMA2X2_EN_INT1_PAD_FLAT__MSK        0x80
#define BMA2X2_EN_INT1_PAD_FLAT__REG        BMA2X2_INT1_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_LOWG__POS        0
#define BMA2X2_EN_INT2_PAD_LOWG__LEN        1
#define BMA2X2_EN_INT2_PAD_LOWG__MSK        0x01
#define BMA2X2_EN_INT2_PAD_LOWG__REG        BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_HIGHG__POS       1
#define BMA2X2_EN_INT2_PAD_HIGHG__LEN       1
#define BMA2X2_EN_INT2_PAD_HIGHG__MSK       0x02
#define BMA2X2_EN_INT2_PAD_HIGHG__REG       BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_SLOPE__POS       2
#define BMA2X2_EN_INT2_PAD_SLOPE__LEN       1
#define BMA2X2_EN_INT2_PAD_SLOPE__MSK       0x04
#define BMA2X2_EN_INT2_PAD_SLOPE__REG       BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_SLO_NO_MOT__POS        3
#define BMA2X2_EN_INT2_PAD_SLO_NO_MOT__LEN        1
#define BMA2X2_EN_INT2_PAD_SLO_NO_MOT__MSK        0x08
#define BMA2X2_EN_INT2_PAD_SLO_NO_MOT__REG        BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_DB_TAP__POS      4
#define BMA2X2_EN_INT2_PAD_DB_TAP__LEN      1
#define BMA2X2_EN_INT2_PAD_DB_TAP__MSK      0x10
#define BMA2X2_EN_INT2_PAD_DB_TAP__REG      BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_SNG_TAP__POS     5
#define BMA2X2_EN_INT2_PAD_SNG_TAP__LEN     1
#define BMA2X2_EN_INT2_PAD_SNG_TAP__MSK     0x20
#define BMA2X2_EN_INT2_PAD_SNG_TAP__REG     BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_ORIENT__POS      6
#define BMA2X2_EN_INT2_PAD_ORIENT__LEN      1
#define BMA2X2_EN_INT2_PAD_ORIENT__MSK      0x40
#define BMA2X2_EN_INT2_PAD_ORIENT__REG      BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT2_PAD_FLAT__POS        7
#define BMA2X2_EN_INT2_PAD_FLAT__LEN        1
#define BMA2X2_EN_INT2_PAD_FLAT__MSK        0x80
#define BMA2X2_EN_INT2_PAD_FLAT__REG        BMA2X2_INT2_PAD_SEL_REG

#define BMA2X2_EN_INT1_PAD_NEWDATA__POS     0
#define BMA2X2_EN_INT1_PAD_NEWDATA__LEN     1
#define BMA2X2_EN_INT1_PAD_NEWDATA__MSK     0x01
#define BMA2X2_EN_INT1_PAD_NEWDATA__REG     BMA2X2_INT_DATA_SEL_REG

#define BMA2X2_EN_INT1_PAD_FWM__POS     1
#define BMA2X2_EN_INT1_PAD_FWM__LEN     1
#define BMA2X2_EN_INT1_PAD_FWM__MSK     0x02
#define BMA2X2_EN_INT1_PAD_FWM__REG     BMA2X2_INT_DATA_SEL_REG

#define BMA2X2_EN_INT1_PAD_FFULL__POS     2
#define BMA2X2_EN_INT1_PAD_FFULL__LEN     1
#define BMA2X2_EN_INT1_PAD_FFULL__MSK     0x04
#define BMA2X2_EN_INT1_PAD_FFULL__REG     BMA2X2_INT_DATA_SEL_REG

#define BMA2X2_EN_INT2_PAD_FFULL__POS     5
#define BMA2X2_EN_INT2_PAD_FFULL__LEN     1
#define BMA2X2_EN_INT2_PAD_FFULL__MSK     0x20
#define BMA2X2_EN_INT2_PAD_FFULL__REG     BMA2X2_INT_DATA_SEL_REG

#define BMA2X2_EN_INT2_PAD_FWM__POS     6
#define BMA2X2_EN_INT2_PAD_FWM__LEN     1
#define BMA2X2_EN_INT2_PAD_FWM__MSK     0x40
#define BMA2X2_EN_INT2_PAD_FWM__REG     BMA2X2_INT_DATA_SEL_REG

#define BMA2X2_EN_INT2_PAD_NEWDATA__POS     7
#define BMA2X2_EN_INT2_PAD_NEWDATA__LEN     1
#define BMA2X2_EN_INT2_PAD_NEWDATA__MSK     0x80
#define BMA2X2_EN_INT2_PAD_NEWDATA__REG     BMA2X2_INT_DATA_SEL_REG

#define BMA2X2_UNFILT_INT_SRC_LOWG__POS        0
#define BMA2X2_UNFILT_INT_SRC_LOWG__LEN        1
#define BMA2X2_UNFILT_INT_SRC_LOWG__MSK        0x01
#define BMA2X2_UNFILT_INT_SRC_LOWG__REG        BMA2X2_INT_SRC_REG

#define BMA2X2_UNFILT_INT_SRC_HIGHG__POS       1
#define BMA2X2_UNFILT_INT_SRC_HIGHG__LEN       1
#define BMA2X2_UNFILT_INT_SRC_HIGHG__MSK       0x02
#define BMA2X2_UNFILT_INT_SRC_HIGHG__REG       BMA2X2_INT_SRC_REG

#define BMA2X2_UNFILT_INT_SRC_SLOPE__POS       2
#define BMA2X2_UNFILT_INT_SRC_SLOPE__LEN       1
#define BMA2X2_UNFILT_INT_SRC_SLOPE__MSK       0x04
#define BMA2X2_UNFILT_INT_SRC_SLOPE__REG       BMA2X2_INT_SRC_REG

#define BMA2X2_UNFILT_INT_SRC_SLO_NO_MOT__POS        3
#define BMA2X2_UNFILT_INT_SRC_SLO_NO_MOT__LEN        1
#define BMA2X2_UNFILT_INT_SRC_SLO_NO_MOT__MSK        0x08
#define BMA2X2_UNFILT_INT_SRC_SLO_NO_MOT__REG        BMA2X2_INT_SRC_REG

#define BMA2X2_UNFILT_INT_SRC_TAP__POS         4
#define BMA2X2_UNFILT_INT_SRC_TAP__LEN         1
#define BMA2X2_UNFILT_INT_SRC_TAP__MSK         0x10
#define BMA2X2_UNFILT_INT_SRC_TAP__REG         BMA2X2_INT_SRC_REG

#define BMA2X2_UNFILT_INT_SRC_DATA__POS        5
#define BMA2X2_UNFILT_INT_SRC_DATA__LEN        1
#define BMA2X2_UNFILT_INT_SRC_DATA__MSK        0x20
#define BMA2X2_UNFILT_INT_SRC_DATA__REG        BMA2X2_INT_SRC_REG

#define BMA2X2_INT1_PAD_ACTIVE_LEVEL__POS       0
#define BMA2X2_INT1_PAD_ACTIVE_LEVEL__LEN       1
#define BMA2X2_INT1_PAD_ACTIVE_LEVEL__MSK       0x01
#define BMA2X2_INT1_PAD_ACTIVE_LEVEL__REG       BMA2X2_INT_SET_REG

#define BMA2X2_INT2_PAD_ACTIVE_LEVEL__POS       2
#define BMA2X2_INT2_PAD_ACTIVE_LEVEL__LEN       1
#define BMA2X2_INT2_PAD_ACTIVE_LEVEL__MSK       0x04
#define BMA2X2_INT2_PAD_ACTIVE_LEVEL__REG       BMA2X2_INT_SET_REG

#define BMA2X2_INT1_PAD_OUTPUT_TYPE__POS        1
#define BMA2X2_INT1_PAD_OUTPUT_TYPE__LEN        1
#define BMA2X2_INT1_PAD_OUTPUT_TYPE__MSK        0x02
#define BMA2X2_INT1_PAD_OUTPUT_TYPE__REG        BMA2X2_INT_SET_REG

#define BMA2X2_INT2_PAD_OUTPUT_TYPE__POS        3
#define BMA2X2_INT2_PAD_OUTPUT_TYPE__LEN        1
#define BMA2X2_INT2_PAD_OUTPUT_TYPE__MSK        0x08
#define BMA2X2_INT2_PAD_OUTPUT_TYPE__REG        BMA2X2_INT_SET_REG

#define BMA2X2_INT_MODE_SEL__POS                0
#define BMA2X2_INT_MODE_SEL__LEN                4
#define BMA2X2_INT_MODE_SEL__MSK                0x0F
#define BMA2X2_INT_MODE_SEL__REG                BMA2X2_INT_CTRL_REG

#define BMA2X2_RESET_INT__POS           7
#define BMA2X2_RESET_INT__LEN           1
#define BMA2X2_RESET_INT__MSK           0x80
#define BMA2X2_RESET_INT__REG           BMA2X2_INT_CTRL_REG

#define BMA2X2_LOWG_DUR__POS                    0
#define BMA2X2_LOWG_DUR__LEN                    8
#define BMA2X2_LOWG_DUR__MSK                    0xFF
#define BMA2X2_LOWG_DUR__REG                    BMA2X2_LOW_DURN_REG

#define BMA2X2_LOWG_THRES__POS                  0
#define BMA2X2_LOWG_THRES__LEN                  8
#define BMA2X2_LOWG_THRES__MSK                  0xFF
#define BMA2X2_LOWG_THRES__REG                  BMA2X2_LOW_THRES_REG

#define BMA2X2_LOWG_HYST__POS                   0
#define BMA2X2_LOWG_HYST__LEN                   2
#define BMA2X2_LOWG_HYST__MSK                   0x03
#define BMA2X2_LOWG_HYST__REG                   BMA2X2_LOW_HIGH_HYST_REG

#define BMA2X2_LOWG_INT_MODE__POS               2
#define BMA2X2_LOWG_INT_MODE__LEN               1
#define BMA2X2_LOWG_INT_MODE__MSK               0x04
#define BMA2X2_LOWG_INT_MODE__REG               BMA2X2_LOW_HIGH_HYST_REG

#define BMA2X2_HIGHG_DUR__POS                    0
#define BMA2X2_HIGHG_DUR__LEN                    8
#define BMA2X2_HIGHG_DUR__MSK                    0xFF
#define BMA2X2_HIGHG_DUR__REG                    BMA2X2_HIGH_DURN_REG

#define BMA2X2_HIGHG_THRES__POS                  0
#define BMA2X2_HIGHG_THRES__LEN                  8
#define BMA2X2_HIGHG_THRES__MSK                  0xFF
#define BMA2X2_HIGHG_THRES__REG                  BMA2X2_HIGH_THRES_REG

#define BMA2X2_HIGHG_HYST__POS                  6
#define BMA2X2_HIGHG_HYST__LEN                  2
#define BMA2X2_HIGHG_HYST__MSK                  0xC0
#define BMA2X2_HIGHG_HYST__REG                  BMA2X2_LOW_HIGH_HYST_REG

#define BMA2X2_SLOPE_DUR__POS                    0
#define BMA2X2_SLOPE_DUR__LEN                    2
#define BMA2X2_SLOPE_DUR__MSK                    0x03
#define BMA2X2_SLOPE_DUR__REG                    BMA2X2_SLOPE_DURN_REG

#define BMA2X2_SLO_NO_MOT_DUR__POS                    2
#define BMA2X2_SLO_NO_MOT_DUR__LEN                    6
#define BMA2X2_SLO_NO_MOT_DUR__MSK                    0xFC
#define BMA2X2_SLO_NO_MOT_DUR__REG                    BMA2X2_SLOPE_DURN_REG

#define BMA2X2_SLOPE_THRES__POS                  0
#define BMA2X2_SLOPE_THRES__LEN                  8
#define BMA2X2_SLOPE_THRES__MSK                  0xFF
#define BMA2X2_SLOPE_THRES__REG                  BMA2X2_SLOPE_THRES_REG

#define BMA2X2_SLO_NO_MOT_THRES__POS                  0
#define BMA2X2_SLO_NO_MOT_THRES__LEN                  8
#define BMA2X2_SLO_NO_MOT_THRES__MSK                  0xFF
#define BMA2X2_SLO_NO_MOT_THRES__REG           BMA2X2_SLO_NO_MOT_THRES_REG

#define BMA2X2_TAP_DUR__POS                    0
#define BMA2X2_TAP_DUR__LEN                    3
#define BMA2X2_TAP_DUR__MSK                    0x07
#define BMA2X2_TAP_DUR__REG                    BMA2X2_TAP_PARAM_REG

#define BMA2X2_TAP_SHOCK_DURN__POS             6
#define BMA2X2_TAP_SHOCK_DURN__LEN             1
#define BMA2X2_TAP_SHOCK_DURN__MSK             0x40
#define BMA2X2_TAP_SHOCK_DURN__REG             BMA2X2_TAP_PARAM_REG

#define BMA2X2_ADV_TAP_INT__POS                5
#define BMA2X2_ADV_TAP_INT__LEN                1
#define BMA2X2_ADV_TAP_INT__MSK                0x20
#define BMA2X2_ADV_TAP_INT__REG                BMA2X2_TAP_PARAM_REG

#define BMA2X2_TAP_QUIET_DURN__POS             7
#define BMA2X2_TAP_QUIET_DURN__LEN             1
#define BMA2X2_TAP_QUIET_DURN__MSK             0x80
#define BMA2X2_TAP_QUIET_DURN__REG             BMA2X2_TAP_PARAM_REG

#define BMA2X2_TAP_THRES__POS                  0
#define BMA2X2_TAP_THRES__LEN                  5
#define BMA2X2_TAP_THRES__MSK                  0x1F
#define BMA2X2_TAP_THRES__REG                  BMA2X2_TAP_THRES_REG

#define BMA2X2_TAP_SAMPLES__POS                6
#define BMA2X2_TAP_SAMPLES__LEN                2
#define BMA2X2_TAP_SAMPLES__MSK                0xC0
#define BMA2X2_TAP_SAMPLES__REG                BMA2X2_TAP_THRES_REG

#define BMA2X2_ORIENT_MODE__POS                  0
#define BMA2X2_ORIENT_MODE__LEN                  2
#define BMA2X2_ORIENT_MODE__MSK                  0x03
#define BMA2X2_ORIENT_MODE__REG                  BMA2X2_ORIENT_PARAM_REG

#define BMA2X2_ORIENT_BLOCK__POS                 2
#define BMA2X2_ORIENT_BLOCK__LEN                 2
#define BMA2X2_ORIENT_BLOCK__MSK                 0x0C
#define BMA2X2_ORIENT_BLOCK__REG                 BMA2X2_ORIENT_PARAM_REG

#define BMA2X2_ORIENT_HYST__POS                  4
#define BMA2X2_ORIENT_HYST__LEN                  3
#define BMA2X2_ORIENT_HYST__MSK                  0x70
#define BMA2X2_ORIENT_HYST__REG                  BMA2X2_ORIENT_PARAM_REG

#define BMA2X2_ORIENT_AXIS__POS                  7
#define BMA2X2_ORIENT_AXIS__LEN                  1
#define BMA2X2_ORIENT_AXIS__MSK                  0x80
#define BMA2X2_ORIENT_AXIS__REG                  BMA2X2_THETA_BLOCK_REG

#define BMA2X2_ORIENT_UD_EN__POS                  6
#define BMA2X2_ORIENT_UD_EN__LEN                  1
#define BMA2X2_ORIENT_UD_EN__MSK                  0x40
#define BMA2X2_ORIENT_UD_EN__REG                  BMA2X2_THETA_BLOCK_REG

#define BMA2X2_THETA_BLOCK__POS                  0
#define BMA2X2_THETA_BLOCK__LEN                  6
#define BMA2X2_THETA_BLOCK__MSK                  0x3F
#define BMA2X2_THETA_BLOCK__REG                  BMA2X2_THETA_BLOCK_REG

#define BMA2X2_THETA_FLAT__POS                  0
#define BMA2X2_THETA_FLAT__LEN                  6
#define BMA2X2_THETA_FLAT__MSK                  0x3F
#define BMA2X2_THETA_FLAT__REG                  BMA2X2_THETA_FLAT_REG

#define BMA2X2_FLAT_HOLD_TIME__POS              4
#define BMA2X2_FLAT_HOLD_TIME__LEN              2
#define BMA2X2_FLAT_HOLD_TIME__MSK              0x30
#define BMA2X2_FLAT_HOLD_TIME__REG              BMA2X2_FLAT_HOLD_TIME_REG

#define BMA2X2_FLAT_HYS__POS                   0
#define BMA2X2_FLAT_HYS__LEN                   3
#define BMA2X2_FLAT_HYS__MSK                   0x07
#define BMA2X2_FLAT_HYS__REG                   BMA2X2_FLAT_HOLD_TIME_REG

#define BMA2X2_FIFO_WML_TRIG_RETAIN__POS                   0
#define BMA2X2_FIFO_WML_TRIG_RETAIN__LEN                   6
#define BMA2X2_FIFO_WML_TRIG_RETAIN__MSK                   0x3F
#define BMA2X2_FIFO_WML_TRIG_RETAIN__REG                   BMA2X2_FIFO_WML_TRIG

#define BMA2X2_EN_SELF_TEST__POS                0
#define BMA2X2_EN_SELF_TEST__LEN                2
#define BMA2X2_EN_SELF_TEST__MSK                0x03
#define BMA2X2_EN_SELF_TEST__REG                BMA2X2_SELF_TEST_REG

#define BMA2X2_NEG_SELF_TEST__POS               2
#define BMA2X2_NEG_SELF_TEST__LEN               1
#define BMA2X2_NEG_SELF_TEST__MSK               0x04
#define BMA2X2_NEG_SELF_TEST__REG               BMA2X2_SELF_TEST_REG

#define BMA2X2_SELF_TEST_AMP__POS               4
#define BMA2X2_SELF_TEST_AMP__LEN               1
#define BMA2X2_SELF_TEST_AMP__MSK               0x10
#define BMA2X2_SELF_TEST_AMP__REG               BMA2X2_SELF_TEST_REG


#define BMA2X2_UNLOCK_EE_PROG_MODE__POS     0
#define BMA2X2_UNLOCK_EE_PROG_MODE__LEN     1
#define BMA2X2_UNLOCK_EE_PROG_MODE__MSK     0x01
#define BMA2X2_UNLOCK_EE_PROG_MODE__REG     BMA2X2_EEPROM_CTRL_REG

#define BMA2X2_START_EE_PROG_TRIG__POS      1
#define BMA2X2_START_EE_PROG_TRIG__LEN      1
#define BMA2X2_START_EE_PROG_TRIG__MSK      0x02
#define BMA2X2_START_EE_PROG_TRIG__REG      BMA2X2_EEPROM_CTRL_REG

#define BMA2X2_EE_PROG_READY__POS          2
#define BMA2X2_EE_PROG_READY__LEN          1
#define BMA2X2_EE_PROG_READY__MSK          0x04
#define BMA2X2_EE_PROG_READY__REG          BMA2X2_EEPROM_CTRL_REG

#define BMA2X2_UPDATE_IMAGE__POS                3
#define BMA2X2_UPDATE_IMAGE__LEN                1
#define BMA2X2_UPDATE_IMAGE__MSK                0x08
#define BMA2X2_UPDATE_IMAGE__REG                BMA2X2_EEPROM_CTRL_REG

#define BMA2X2_EE_REMAIN__POS                4
#define BMA2X2_EE_REMAIN__LEN                4
#define BMA2X2_EE_REMAIN__MSK                0xF0
#define BMA2X2_EE_REMAIN__REG                BMA2X2_EEPROM_CTRL_REG

#define BMA2X2_EN_SPI_MODE_3__POS              0
#define BMA2X2_EN_SPI_MODE_3__LEN              1
#define BMA2X2_EN_SPI_MODE_3__MSK              0x01
#define BMA2X2_EN_SPI_MODE_3__REG              BMA2X2_SERIAL_CTRL_REG

#define BMA2X2_I2C_WATCHDOG_PERIOD__POS        1
#define BMA2X2_I2C_WATCHDOG_PERIOD__LEN        1
#define BMA2X2_I2C_WATCHDOG_PERIOD__MSK        0x02
#define BMA2X2_I2C_WATCHDOG_PERIOD__REG        BMA2X2_SERIAL_CTRL_REG

#define BMA2X2_EN_I2C_WATCHDOG__POS            2
#define BMA2X2_EN_I2C_WATCHDOG__LEN            1
#define BMA2X2_EN_I2C_WATCHDOG__MSK            0x04
#define BMA2X2_EN_I2C_WATCHDOG__REG            BMA2X2_SERIAL_CTRL_REG

#define BMA2X2_EXT_MODE__POS              7
#define BMA2X2_EXT_MODE__LEN              1
#define BMA2X2_EXT_MODE__MSK              0x80
#define BMA2X2_EXT_MODE__REG              BMA2X2_EXTMODE_CTRL_REG

#define BMA2X2_ALLOW_UPPER__POS        6
#define BMA2X2_ALLOW_UPPER__LEN        1
#define BMA2X2_ALLOW_UPPER__MSK        0x40
#define BMA2X2_ALLOW_UPPER__REG        BMA2X2_EXTMODE_CTRL_REG

#define BMA2X2_MAP_2_LOWER__POS            5
#define BMA2X2_MAP_2_LOWER__LEN            1
#define BMA2X2_MAP_2_LOWER__MSK            0x20
#define BMA2X2_MAP_2_LOWER__REG            BMA2X2_EXTMODE_CTRL_REG

#define BMA2X2_MAGIC_NUMBER__POS            0
#define BMA2X2_MAGIC_NUMBER__LEN            5
#define BMA2X2_MAGIC_NUMBER__MSK            0x1F
#define BMA2X2_MAGIC_NUMBER__REG            BMA2X2_EXTMODE_CTRL_REG

#define BMA2X2_UNLOCK_EE_WRITE_TRIM__POS        4
#define BMA2X2_UNLOCK_EE_WRITE_TRIM__LEN        4
#define BMA2X2_UNLOCK_EE_WRITE_TRIM__MSK        0xF0
#define BMA2X2_UNLOCK_EE_WRITE_TRIM__REG        BMA2X2_CTRL_UNLOCK_REG

#define BMA2X2_EN_SLOW_COMP_X__POS              0
#define BMA2X2_EN_SLOW_COMP_X__LEN              1
#define BMA2X2_EN_SLOW_COMP_X__MSK              0x01
#define BMA2X2_EN_SLOW_COMP_X__REG              BMA2X2_OFFSET_CTRL_REG

#define BMA2X2_EN_SLOW_COMP_Y__POS              1
#define BMA2X2_EN_SLOW_COMP_Y__LEN              1
#define BMA2X2_EN_SLOW_COMP_Y__MSK              0x02
#define BMA2X2_EN_SLOW_COMP_Y__REG              BMA2X2_OFFSET_CTRL_REG

#define BMA2X2_EN_SLOW_COMP_Z__POS              2
#define BMA2X2_EN_SLOW_COMP_Z__LEN              1
#define BMA2X2_EN_SLOW_COMP_Z__MSK              0x04
#define BMA2X2_EN_SLOW_COMP_Z__REG              BMA2X2_OFFSET_CTRL_REG

#define BMA2X2_FAST_CAL_RDY_S__POS             4
#define BMA2X2_FAST_CAL_RDY_S__LEN             1
#define BMA2X2_FAST_CAL_RDY_S__MSK             0x10
#define BMA2X2_FAST_CAL_RDY_S__REG             BMA2X2_OFFSET_CTRL_REG

#define BMA2X2_CAL_TRIGGER__POS                5
#define BMA2X2_CAL_TRIGGER__LEN                2
#define BMA2X2_CAL_TRIGGER__MSK                0x60
#define BMA2X2_CAL_TRIGGER__REG                BMA2X2_OFFSET_CTRL_REG

#define BMA2X2_RESET_OFFSET_REGS__POS           7
#define BMA2X2_RESET_OFFSET_REGS__LEN           1
#define BMA2X2_RESET_OFFSET_REGS__MSK           0x80
#define BMA2X2_RESET_OFFSET_REGS__REG           BMA2X2_OFFSET_CTRL_REG

#define BMA2X2_COMP_CUTOFF__POS                 0
#define BMA2X2_COMP_CUTOFF__LEN                 1
#define BMA2X2_COMP_CUTOFF__MSK                 0x01
#define BMA2X2_COMP_CUTOFF__REG                 BMA2X2_OFFSET_PARAMS_REG

#define BMA2X2_COMP_TARGET_OFFSET_X__POS        1
#define BMA2X2_COMP_TARGET_OFFSET_X__LEN        2
#define BMA2X2_COMP_TARGET_OFFSET_X__MSK        0x06
#define BMA2X2_COMP_TARGET_OFFSET_X__REG        BMA2X2_OFFSET_PARAMS_REG

#define BMA2X2_COMP_TARGET_OFFSET_Y__POS        3
#define BMA2X2_COMP_TARGET_OFFSET_Y__LEN        2
#define BMA2X2_COMP_TARGET_OFFSET_Y__MSK        0x18
#define BMA2X2_COMP_TARGET_OFFSET_Y__REG        BMA2X2_OFFSET_PARAMS_REG

#define BMA2X2_COMP_TARGET_OFFSET_Z__POS        5
#define BMA2X2_COMP_TARGET_OFFSET_Z__LEN        2
#define BMA2X2_COMP_TARGET_OFFSET_Z__MSK        0x60
#define BMA2X2_COMP_TARGET_OFFSET_Z__REG        BMA2X2_OFFSET_PARAMS_REG

#define BMA2X2_FIFO_DATA_SELECT__POS                 0
#define BMA2X2_FIFO_DATA_SELECT__LEN                 2
#define BMA2X2_FIFO_DATA_SELECT__MSK                 0x03
#define BMA2X2_FIFO_DATA_SELECT__REG                 BMA2X2_FIFO_MODE_REG

#define BMA2X2_FIFO_TRIGGER_SOURCE__POS                 2
#define BMA2X2_FIFO_TRIGGER_SOURCE__LEN                 2
#define BMA2X2_FIFO_TRIGGER_SOURCE__MSK                 0x0C
#define BMA2X2_FIFO_TRIGGER_SOURCE__REG                 BMA2X2_FIFO_MODE_REG

#define BMA2X2_FIFO_TRIGGER_ACTION__POS                 4
#define BMA2X2_FIFO_TRIGGER_ACTION__LEN                 2
#define BMA2X2_FIFO_TRIGGER_ACTION__MSK                 0x30
#define BMA2X2_FIFO_TRIGGER_ACTION__REG                 BMA2X2_FIFO_MODE_REG

#define BMA2X2_FIFO_MODE__POS                 6
#define BMA2X2_FIFO_MODE__LEN                 2
#define BMA2X2_FIFO_MODE__MSK                 0xC0
#define BMA2X2_FIFO_MODE__REG                 BMA2X2_FIFO_MODE_REG


#define BMA2X2_STATUS1                             0
#define BMA2X2_STATUS2                             1
#define BMA2X2_STATUS3                             2
#define BMA2X2_STATUS4                             3
#define BMA2X2_STATUS5                             4


#define BMA2X2_RANGE_2G                 3
#define BMA2X2_RANGE_4G                 5
#define BMA2X2_RANGE_8G                 8
#define BMA2X2_RANGE_16G                12


#define BMA2X2_BW_7_81HZ        0x08
#define BMA2X2_BW_15_63HZ       0x09
#define BMA2X2_BW_31_25HZ       0x0A
#define BMA2X2_BW_62_50HZ       0x0B
#define BMA2X2_BW_125HZ         0x0C
#define BMA2X2_BW_250HZ         0x0D
#define BMA2X2_BW_500HZ         0x0E
#define BMA2X2_BW_1000HZ        0x0F

#define BMA2X2_SLEEP_DUR_0_5MS        0x05
#define BMA2X2_SLEEP_DUR_1MS          0x06
#define BMA2X2_SLEEP_DUR_2MS          0x07
#define BMA2X2_SLEEP_DUR_4MS          0x08
#define BMA2X2_SLEEP_DUR_6MS          0x09
#define BMA2X2_SLEEP_DUR_10MS         0x0A
#define BMA2X2_SLEEP_DUR_25MS         0x0B
#define BMA2X2_SLEEP_DUR_50MS         0x0C
#define BMA2X2_SLEEP_DUR_100MS        0x0D
#define BMA2X2_SLEEP_DUR_500MS        0x0E
#define BMA2X2_SLEEP_DUR_1S           0x0F

#define BMA2X2_LATCH_DUR_NON_LATCH    0x00
#define BMA2X2_LATCH_DUR_250MS        0x01
#define BMA2X2_LATCH_DUR_500MS        0x02
#define BMA2X2_LATCH_DUR_1S           0x03
#define BMA2X2_LATCH_DUR_2S           0x04
#define BMA2X2_LATCH_DUR_4S           0x05
#define BMA2X2_LATCH_DUR_8S           0x06
#define BMA2X2_LATCH_DUR_LATCH        0x07
#define BMA2X2_LATCH_DUR_NON_LATCH1   0x08
#define BMA2X2_LATCH_DUR_250US        0x09
#define BMA2X2_LATCH_DUR_500US        0x0A
#define BMA2X2_LATCH_DUR_1MS          0x0B
#define BMA2X2_LATCH_DUR_12_5MS       0x0C
#define BMA2X2_LATCH_DUR_25MS         0x0D
#define BMA2X2_LATCH_DUR_50MS         0x0E
#define BMA2X2_LATCH_DUR_LATCH1       0x0F

#define BMA2X2_MODE_NORMAL             0
#define BMA2X2_MODE_LOWPOWER1          1
#define BMA2X2_MODE_SUSPEND            2
#define BMA2X2_MODE_DEEP_SUSPEND       3
#define BMA2X2_MODE_LOWPOWER2          4
#define BMA2X2_MODE_STANDBY            5

#define BMA2X2_X_AXIS           0
#define BMA2X2_Y_AXIS           1
#define BMA2X2_Z_AXIS           2

#define BMA2X2_Low_G_Interrupt       0
#define BMA2X2_High_G_X_Interrupt    1
#define BMA2X2_High_G_Y_Interrupt    2
#define BMA2X2_High_G_Z_Interrupt    3
#define BMA2X2_DATA_EN               4
#define BMA2X2_Slope_X_Interrupt     5
#define BMA2X2_Slope_Y_Interrupt     6
#define BMA2X2_Slope_Z_Interrupt     7
#define BMA2X2_Single_Tap_Interrupt  8
#define BMA2X2_Double_Tap_Interrupt  9
#define BMA2X2_Orient_Interrupt      10
#define BMA2X2_Flat_Interrupt        11
#define BMA2X2_FFULL_INTERRUPT       12
#define BMA2X2_FWM_INTERRUPT         13

#define BMA2X2_INT1_LOWG         0
#define BMA2X2_INT2_LOWG         1
#define BMA2X2_INT1_HIGHG        0
#define BMA2X2_INT2_HIGHG        1
#define BMA2X2_INT1_SLOPE        0
#define BMA2X2_INT2_SLOPE        1
#define BMA2X2_INT1_SLO_NO_MOT   0
#define BMA2X2_INT2_SLO_NO_MOT   1
#define BMA2X2_INT1_DTAP         0
#define BMA2X2_INT2_DTAP         1
#define BMA2X2_INT1_STAP         0
#define BMA2X2_INT2_STAP         1
#define BMA2X2_INT1_ORIENT       0
#define BMA2X2_INT2_ORIENT       1
#define BMA2X2_INT1_FLAT         0
#define BMA2X2_INT2_FLAT         1
#define BMA2X2_INT1_NDATA        0
#define BMA2X2_INT2_NDATA        1
#define BMA2X2_INT1_FWM          0
#define BMA2X2_INT2_FWM          1
#define BMA2X2_INT1_FFULL        0
#define BMA2X2_INT2_FFULL        1

#define BMA2X2_SRC_LOWG         0
#define BMA2X2_SRC_HIGHG        1
#define BMA2X2_SRC_SLOPE        2
#define BMA2X2_SRC_SLO_NO_MOT   3
#define BMA2X2_SRC_TAP          4
#define BMA2X2_SRC_DATA         5

#define BMA2X2_INT1_OUTPUT      0
#define BMA2X2_INT2_OUTPUT      1
#define BMA2X2_INT1_LEVEL       0
#define BMA2X2_INT2_LEVEL       1

#define BMA2X2_LOW_DURATION            0
#define BMA2X2_HIGH_DURATION           1
#define BMA2X2_SLOPE_DURATION          2
#define BMA2X2_SLO_NO_MOT_DURATION     3

#define BMA2X2_LOW_THRESHOLD            0
#define BMA2X2_HIGH_THRESHOLD           1
#define BMA2X2_SLOPE_THRESHOLD          2
#define BMA2X2_SLO_NO_MOT_THRESHOLD     3


#define BMA2X2_LOWG_HYST                0
#define BMA2X2_HIGHG_HYST               1

#define BMA2X2_ORIENT_THETA             0
#define BMA2X2_FLAT_THETA               1

#define BMA2X2_I2C_SELECT               0
#define BMA2X2_I2C_EN                   1

#define BMA2X2_SLOW_COMP_X              0
#define BMA2X2_SLOW_COMP_Y              1
#define BMA2X2_SLOW_COMP_Z              2

#define BMA2X2_CUT_OFF                  0
#define BMA2X2_OFFSET_TRIGGER_X         1
#define BMA2X2_OFFSET_TRIGGER_Y         2
#define BMA2X2_OFFSET_TRIGGER_Z         3

#define BMA2X2_GP0                      0
#define BMA2X2_GP1                      1

#define BMA2X2_SLO_NO_MOT_EN_X          0
#define BMA2X2_SLO_NO_MOT_EN_Y          1
#define BMA2X2_SLO_NO_MOT_EN_Z          2
#define BMA2X2_SLO_NO_MOT_EN_SEL        3

#define BMA2X2_WAKE_UP_DUR_20MS         0
#define BMA2X2_WAKE_UP_DUR_80MS         1
#define BMA2X2_WAKE_UP_DUR_320MS                2
#define BMA2X2_WAKE_UP_DUR_2560MS               3

#define BMA2X2_SELF_TEST0_ON            1
#define BMA2X2_SELF_TEST1_ON            2

#define BMA2X2_EE_W_OFF                 0
#define BMA2X2_EE_W_ON                  1

#define BMA2X2_LOW_TH_IN_G(gthres, range)           ((256 * gthres) / range)


#define BMA2X2_HIGH_TH_IN_G(gthres, range)          ((256 * gthres) / range)


#define BMA2X2_LOW_HY_IN_G(ghyst, range)            ((32 * ghyst) / range)


#define BMA2X2_HIGH_HY_IN_G(ghyst, range)           ((32 * ghyst) / range)


#define BMA2X2_SLOPE_TH_IN_G(gthres, range)    ((128 * gthres) / range)


#define BMA2X2_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)


#define BMA2X2_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#define CHECK_CHIP_ID_TIME_MAX 5
#define BMA255_CHIP_ID 0XFA
#define BMA250E_CHIP_ID 0XF9
#define BMA222E_CHIP_ID 0XF8
#define BMA280_CHIP_ID 0XFB
#define BMA355_CHIP_ID 0XEA

#define BMA255_TYPE 0
#define BMA250E_TYPE 1
#define BMA222E_TYPE 2
#define BMA280_TYPE 3

#define MAX_FIFO_F_LEVEL 32
#define MAX_FIFO_F_BYTES 6
#define BMA_MAX_RETRY_I2C_XFER (100)

/*! Bosch sensor unknown place*/
#define BOSCH_SENSOR_PLACE_UNKNOWN (-1)
/*! Bosch sensor remapping table size P0~P7*/
#define MAX_AXIS_REMAP_TAB_SZ 8

/* How was BMA enabled(set to operation mode) */
#define BMA_ENABLED_ALL 0
#define BMA_ENABLED_SGM 1
#define BMA_ENABLED_DTAP 2
#define BMA_ENABLED_INPUT 3
#define BMA_ENABLED_BSX 4

#define BMA2X2_FIFO_DAT_SEL_X                     1
#define BMA2X2_FIFO_DAT_SEL_Y                     2
#define BMA2X2_FIFO_DAT_SEL_Z                     3

#ifdef CONFIG_SENSORS_BMI058
#define C_BMI058_One_U8X                                 1
#define C_BMI058_Two_U8X                                 2
#define BMI058_OFFSET_TRIGGER_X                BMA2X2_OFFSET_TRIGGER_Y
#define BMI058_OFFSET_TRIGGER_Y                BMA2X2_OFFSET_TRIGGER_X

/*! BMI058 X AXIS OFFSET REG definition*/
#define BMI058_OFFSET_X_AXIS_REG              BMA2X2_OFFSET_Y_AXIS_REG
/*! BMI058 Y AXIS OFFSET REG definition*/
#define BMI058_OFFSET_Y_AXIS_REG              BMA2X2_OFFSET_X_AXIS_REG

#define BMI058_FIFO_DAT_SEL_X                       BMA2X2_FIFO_DAT_SEL_Y
#define BMI058_FIFO_DAT_SEL_Y                       BMA2X2_FIFO_DAT_SEL_X

/*! BMA2x2 common slow no motion X interrupt type definition*/
#define BMA2X2_SLOW_NO_MOT_X_INT          12
/*! BMA2x2 common slow no motion Y interrupt type definition*/
#define BMA2X2_SLOW_NO_MOT_Y_INT          13
/*! BMA2x2 common High G X interrupt type definition*/
#define BMA2X2_HIGHG_X_INT          1
/*! BMA2x2 common High G Y interrupt type definition*/
#define BMA2X2_HIGHG_Y_INT          2
/*! BMA2x2 common slope X interrupt type definition*/
#define BMA2X2_SLOPE_X_INT          5
/*! BMA2x2 common slope Y interrupt type definition*/
#define BMA2X2_SLOPE_Y_INT          6

/*! this structure holds some interrupt types difference
**between BMA2x2 and BMI058.
*/
struct interrupt_map_t {
	int x;
	int y;
};
/*!*Need to use BMA2x2 Common interrupt type definition to
* instead of Some of BMI058 reversed Interrupt type
* because of HW Register.
* The reversed Interrupt types contain:
* slow_no_mot_x_int && slow_not_mot_y_int
* highg_x_int && highg_y_int
* slope_x_int && slope_y_int
**/
static const struct interrupt_map_t int_map[] = {
	{BMA2X2_SLOW_NO_MOT_X_INT, BMA2X2_SLOW_NO_MOT_Y_INT},
	{BMA2X2_HIGHG_X_INT, BMA2X2_HIGHG_Y_INT},
	{BMA2X2_SLOPE_X_INT, BMA2X2_SLOPE_Y_INT}
};

/*! high g or slope interrupt type definition for BMI058*/
/*! High G interrupt of x, y, z axis happened */
#define HIGH_G_INTERRUPT_X            HIGH_G_INTERRUPT_Y_HAPPENED
#define HIGH_G_INTERRUPT_Y            HIGH_G_INTERRUPT_X_HAPPENED
#define HIGH_G_INTERRUPT_Z            HIGH_G_INTERRUPT_Z_HAPPENED
/*! High G interrupt of x, y, z negative axis happened */
#define HIGH_G_INTERRUPT_X_N          HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED
#define HIGH_G_INTERRUPT_Y_N          HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED
#define HIGH_G_INTERRUPT_Z_N          HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED
/*! Slope interrupt of x, y, z axis happened */
#define SLOPE_INTERRUPT_X             SLOPE_INTERRUPT_Y_HAPPENED
#define SLOPE_INTERRUPT_Y             SLOPE_INTERRUPT_X_HAPPENED
#define SLOPE_INTERRUPT_Z             SLOPE_INTERRUPT_Z_HAPPENED
/*! Slope interrupt of x, y, z negative axis happened */
#define SLOPE_INTERRUPT_X_N           SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED
#define SLOPE_INTERRUPT_Y_N           SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED
#define SLOPE_INTERRUPT_Z_N           SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED


#else

/*! high g or slope interrupt type definition*/
/*! High G interrupt of x, y, z axis happened */
#define HIGH_G_INTERRUPT_X            HIGH_G_INTERRUPT_X_HAPPENED
#define HIGH_G_INTERRUPT_Y            HIGH_G_INTERRUPT_Y_HAPPENED
#define HIGH_G_INTERRUPT_Z            HIGH_G_INTERRUPT_Z_HAPPENED
/*! High G interrupt of x, y, z negative axis happened */
#define HIGH_G_INTERRUPT_X_N          HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED
#define HIGH_G_INTERRUPT_Y_N          HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED
#define HIGH_G_INTERRUPT_Z_N          HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED
/*! Slope interrupt of x, y, z axis happened */
#define SLOPE_INTERRUPT_X             SLOPE_INTERRUPT_X_HAPPENED
#define SLOPE_INTERRUPT_Y             SLOPE_INTERRUPT_Y_HAPPENED
#define SLOPE_INTERRUPT_Z             SLOPE_INTERRUPT_Z_HAPPENED
/*! Slope interrupt of x, y, z negative axis happened */
#define SLOPE_INTERRUPT_X_N           SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED
#define SLOPE_INTERRUPT_Y_N           SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED
#define SLOPE_INTERRUPT_Z_N           SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED


#endif/*End of CONFIG_SENSORS_BMI058*/

/*! A workaroud mask definition with complete resolution exists
* aim at writing operation FIFO_CONFIG_1, 0x3E register */
#define FIFO_WORKAROUNDS_MSK         BMA2X2_FIFO_TRIGGER_SOURCE__MSK

static int filter_flag;
static unsigned int odr_delay;
static char chip_info[20];
static struct task_struct *thread;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int gsensor_flag;
ktime_t acc_time_monoboot;
int runirq_cnt;
int runthread_cnt;
/* define for input core debug, check nums of reports */
int sync_cnt;
int event_gap_cnt;
unsigned int offset_value[3];
static unsigned long delay_ms;
static int c200;
struct bma2x2_type_map_t {

	/*! bma2x2 sensor chip id */
	uint16_t chip_id;

	/*! bma2x2 sensor type */
	uint16_t sensor_type;

	/*! bma2x2 sensor name */
	const char *sensor_name;
};

static const struct bma2x2_type_map_t sensor_type_map[] = {

	{BMA255_CHIP_ID, BMA255_TYPE, "bma253"},
	{BMA355_CHIP_ID, BMA255_TYPE, "bma355"},
	{BMA250E_CHIP_ID, BMA250E_TYPE, "bma250e"},
	{BMA222E_CHIP_ID, BMA222E_TYPE, "bma222e"},
	{BMA280_CHIP_ID, BMA280_TYPE, "bma280"},

};

/*!
* Bst sensor common definition,
* please give parameters in BSP file.
*/
struct bosch_sensor_specific {
	char *name;
	/* 0 to 7 */
	int place;
	int irq;
	int (*irq_gpio_cfg)(void);
};

/*!
 * we use a typedef to hide the detail,
 * because this type might be changed
 */
struct bosch_sensor_axis_remap {
	/* src means which source will be mapped to target x, y, z axis */
	/* if an target OS axis is remapped from (-)x,
	 * src is 0, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)y,
	 * src is 1, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)z,
	 * src is 2, sign_* is (-)1 */
	unsigned int src_x:3;
	unsigned int src_y:3;
	unsigned int src_z:3;

	signed int sign_x:2;
	signed int sign_y:2;
	signed int sign_z:2;
};

struct bosch_sensor_data {
	union {
		int16_t v[3];
		struct {
			int16_t x;
			int16_t y;
			int16_t z;
		};
	};
};

struct bma2x2acc {
	s16 x;
	s16 y;
	s16 z;
};

typedef struct bma2x2_buf_data_type {
	int32_t x;
	int32_t y;
	int32_t z;
} bma2x2_buf_data_t;

typedef struct bma2x2_buf_type {
	bma2x2_buf_data_t data;
	int64_t timestamp;
} bma2x2_buf_t;

#define BMA2X2_RING_BUF_SIZE 100

static bma2x2_buf_t bma2x2_ring_buf[BMA2X2_RING_BUF_SIZE];
static int s_ring_buf_head;
static int s_ring_buf_tail;

struct bma2x2_data {
	struct i2c_client *bma2x2_client;
	atomic_t delay;
	atomic_t enable;
	unsigned int on_before_suspend;
	atomic_t selftest_result;
	unsigned int chip_id;
	unsigned int fifo_count;
	unsigned char fifo_datasel;
	unsigned char mode;
	signed char sensor_type;
	uint64_t timestamp;
	uint64_t fifo_time;
	uint64_t base_time;
	uint64_t acc_count;
	uint64_t time_odr;
	uint64_t fifo_time2;
	struct work_struct report_data_work;
	int is_timer_running;
	struct hrtimer timer;
	ktime_t work_delay_kt;
	struct input_dev *input;

	struct bst_dev *bst_acc;

	atomic_t i2c_busy;		/* only 0 can suspend */
	struct bma2x2acc value;
	struct mutex value_mutex;
	struct mutex enable_mutex;
	struct mutex mode_mutex;
	struct mutex mutex_ring_buf;
	struct delayed_work work;
	struct work_struct irq_work;
	int gpio_num;
	int irq;
	struct bosch_sensor_specific *bst_pd;

	int bma_mode_enabled;
	struct input_dev *dev_interrupt;
};

static int bma2x2_set_mode(struct i2c_client *client,
			u8 mode, u8 enabled_mode);
static int bma2x2_get_mode(struct i2c_client *client, u8 *mode);
static int bma2x2_get_fifo_mode(struct i2c_client *client, u8 *fifo_mode);
static int bma2x2_set_fifo_mode(struct i2c_client *client, u8 fifo_mode);
static int bma2x2_normal_to_suspend(struct bma2x2_data *bma2x2,
				unsigned char data1, unsigned char data2);
static noinline void bma2x2_update_data(void *u);

/*Remapping for BMA2X2*/

static const struct bosch_sensor_axis_remap
bst_axis_remap_tab_dft[MAX_AXIS_REMAP_TAB_SZ] = {
	/* src_x src_y src_z  sign_x  sign_y  sign_z */
	{  0,    1,    2,     1,      1,      1 }, /* P0 */
	{  1,    0,    2,     1,     -1,      1 }, /* P1 */
	{  0,    1,    2,    -1,     -1,      1 }, /* P2 */
	{  1,    0,    2,    -1,      1,      1 }, /* P3 */

	{  0,    1,    2,    -1,      1,     -1 }, /* P4 */
	{  1,    0,    2,    -1,     -1,     -1 }, /* P5 */
	{  0,    1,    2,     1,     -1,     -1 }, /* P6 */
	{  1,    0,    2,     1,      1,     -1 }, /* P7 */
};

static void bst_remap_sensor_data(struct bosch_sensor_data *data,
		const struct bosch_sensor_axis_remap *remap)
{
	struct bosch_sensor_data tmp;

	tmp.x = data->v[remap->src_x] * remap->sign_x;
	tmp.y = data->v[remap->src_y] * remap->sign_y;
	tmp.z = data->v[remap->src_z] * remap->sign_z;

	memcpy(data, &tmp, sizeof(*data));
}


static void bst_remap_sensor_data_dft_tab(struct bosch_sensor_data *data,
		int place)
{
	/* sensor with place 0 needs not to be remapped */
	if ((place <= 0) || (place >= MAX_AXIS_REMAP_TAB_SZ))
		return;

	bst_remap_sensor_data(data, &bst_axis_remap_tab_dft[place]);
}

static void bma2x2_remap_sensor_data(struct bma2x2acc *val,
		struct bma2x2_data *client_data)
{
	struct bosch_sensor_data bsd;
	int place;

	if ((NULL == client_data->bst_pd) || (BOSCH_SENSOR_PLACE_UNKNOWN
			 == client_data->bst_pd->place))
		place = BOSCH_SENSOR_PLACE_UNKNOWN;
	else
		place = client_data->bst_pd->place;

#ifdef CONFIG_SENSORS_BMI058
/*x,y need to be invesed becase of HW Register for BMI058*/
	bsd.y = val->x;
	bsd.x = val->y;
	bsd.z = val->z;
#else
	bsd.x = val->x;
	bsd.y = val->y;
	bsd.z = val->z;
#endif

	bst_remap_sensor_data_dft_tab(&bsd, place);

	val->x = bsd.x;
	val->y = bsd.y;
	val->z = bsd.z;

}

static int bma2x2_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;
	dummy = i2c_smbus_read_byte_data(client, reg_addr);
	if (dummy < 0)
		return -1;
	*data = dummy & 0x000000ff;

	return 0;
}

static int bma2x2_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;

	dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
	if (dummy < 0)
		return -1;
	udelay(2);
	return 0;
}

static int bma2x2_smbus_read_byte_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	s32 dummy;
	dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len, data);
	/* i2c fail */
	if (dummy < 0) {
		LOG_INFO("exception: iic read error dummy=%d\n", dummy);
		return -1;
	}
	return 0;
}

static int bma_i2c_burst_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u16 len)
{
	int retry;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		},

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	for (retry = 0; retry < BMA_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			mdelay(1);
	}

	if (BMA_MAX_RETRY_I2C_XFER <= retry) {
		LOG_INFO("I2C xfer error");
		return -EIO;
	}

	return 0;
}

static int bma2x2_check_chip_id(struct i2c_client *client,
					struct bma2x2_data *data)
{
	int i = 0;
	int err = 0;
	unsigned char chip_id = 0;
	unsigned char read_count = 0;
	unsigned char bma2x2_sensor_type_count = 0;

	bma2x2_sensor_type_count =
		sizeof(sensor_type_map) / sizeof(struct bma2x2_type_map_t);

	while (read_count++ < CHECK_CHIP_ID_TIME_MAX) {
		if (bma2x2_smbus_read_byte(client, BMA2X2_CHIP_ID_REG,
							&chip_id) < 0) {
			LOG_ERR("Bosch Sensortec Device not found"
			"i2c bus read error, read chip_id:%d\n", chip_id);
			continue;
		} else {
		for (i = 0; i < bma2x2_sensor_type_count; i++) {
			if (sensor_type_map[i].chip_id == chip_id) {
				data->sensor_type =
					sensor_type_map[i].sensor_type;
				data->chip_id = chip_id;
				LOG_INFO("Bosch Sensortec Device detected,"
					" HW IC name: %s\n",
						sensor_type_map[i].sensor_name);
				sprintf(chip_info, "%s",
						sensor_type_map[i].sensor_name);
				filter_flag = 1;
					return err;
			}
		}
		if (i < bma2x2_sensor_type_count)
			return err;
		else {
			if (read_count == CHECK_CHIP_ID_TIME_MAX) {
				LOG_ERR("Failed! Bosch Sensortec Device"
					" not found, mismatch chip_id:%d\n",
								chip_id);
					err = -ENODEV;
					return err;
			}
		}
		mdelay(1);
		}
	}
	return err;
}

#ifdef BMA_ENABLE_NEWDATA_INT
static int bma2x2_set_newdata(struct i2c_client *client,
			unsigned char channel, unsigned char int_newdata)
{

	unsigned char data = 0;
	int comres = 0;

	switch (channel) {
	case BMA2X2_INT1_NDATA:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_NEWDATA__REG, &data);
		data = BMA2X2_SET_BITSLICE(data,
				BMA2X2_EN_INT1_PAD_NEWDATA, int_newdata);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_NEWDATA__REG, &data);
		break;
	case BMA2X2_INT2_NDATA:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_NEWDATA__REG, &data);
		data = BMA2X2_SET_BITSLICE(data,
				BMA2X2_EN_INT2_PAD_NEWDATA, int_newdata);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_NEWDATA__REG, &data);
		break;
	default:
		comres = -1;
		break;
	}

	return comres;

}
#endif /* BMA_ENABLE_NEWDATA_INT */

#ifdef BMA2X2_ENABLE_INT2
static int bma2x2_set_int1_pad_sel(struct i2c_client *client, unsigned char
		int1sel)
{
	int comres = 0;
	unsigned char data = 0;
	unsigned char state;
	state = 0x01;


	switch (int1sel) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_LOWG__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_LOWG,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_LOWG__REG, &data);
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_HIGHG__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_HIGHG,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_HIGHG__REG, &data);
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_SLOPE__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_SLOPE,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_SLOPE__REG, &data);
		break;
	case 3:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_DB_TAP__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_DB_TAP,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_DB_TAP__REG, &data);
		break;
	case 4:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_SNG_TAP__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_SNG_TAP,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_SNG_TAP__REG, &data);
		break;
	case 5:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_ORIENT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_ORIENT,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_ORIENT__REG, &data);
		break;
	case 6:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_FLAT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_FLAT,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_FLAT__REG, &data);
		break;
	case 7:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT1_PAD_SLO_NO_MOT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT1_PAD_SLO_NO_MOT,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT1_PAD_SLO_NO_MOT__REG, &data);
		break;

	default:
		break;
	}

	return comres;
}

static int bma2x2_set_int2_pad_sel(struct i2c_client *client, unsigned char
		int2sel)
{
	int comres = 0;
	unsigned char data = 0;
	unsigned char state;
	state = 0x01;


	switch (int2sel) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_LOWG__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_LOWG,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_LOWG__REG, &data);
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_HIGHG__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_HIGHG,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_HIGHG__REG, &data);
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_SLOPE__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_SLOPE,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_SLOPE__REG, &data);
		break;
	case 3:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_DB_TAP__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_DB_TAP,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_DB_TAP__REG, &data);
		break;
	case 4:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_SNG_TAP__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_SNG_TAP,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_SNG_TAP__REG, &data);
		break;
	case 5:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_ORIENT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_ORIENT,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_ORIENT__REG, &data);
		break;
	case 6:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_FLAT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_FLAT,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_FLAT__REG, &data);
		break;
	case 7:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_EN_INT2_PAD_SLO_NO_MOT__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_INT2_PAD_SLO_NO_MOT,
				state);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_EN_INT2_PAD_SLO_NO_MOT__REG, &data);
		break;
	default:
		break;
	}

	return comres;
}
#endif /* BMA2X2_ENABLE_INT2 */

static int bma2x2_set_Int_Enable(struct i2c_client *client, unsigned char
		InterruptType, unsigned char value)
{
	int comres = 0;
	unsigned char data1 = 0;
	unsigned char data2 = 0;

	LOG_INFO("InterruptType=%d:  value=%d\n", InterruptType, value);
	if ((11 < InterruptType) && (InterruptType < 16)) {
		switch (InterruptType) {
		case 12:
			/* slow/no motion X Interrupt  */
			comres = bma2x2_smbus_read_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_X_INT__REG, &data1);
			data1 = BMA2X2_SET_BITSLICE(data1,
				BMA2X2_INT_SLO_NO_MOT_EN_X_INT, value);
			comres = bma2x2_smbus_write_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_X_INT__REG, &data1);
			break;
		case 13:
			/* slow/no motion Y Interrupt  */
			comres = bma2x2_smbus_read_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_Y_INT__REG, &data1);
			data1 = BMA2X2_SET_BITSLICE(data1,
				BMA2X2_INT_SLO_NO_MOT_EN_Y_INT, value);
			comres = bma2x2_smbus_write_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_Y_INT__REG, &data1);
			break;
		case 14:
			/* slow/no motion Z Interrupt  */
			comres = bma2x2_smbus_read_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_Z_INT__REG, &data1);
			data1 = BMA2X2_SET_BITSLICE(data1,
				BMA2X2_INT_SLO_NO_MOT_EN_Z_INT, value);
			comres = bma2x2_smbus_write_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_Z_INT__REG, &data1);
			break;
		case 15:
			/* slow / no motion Interrupt select */
			comres = bma2x2_smbus_read_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT__REG, &data1);
			data1 = BMA2X2_SET_BITSLICE(data1,
				BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT, value);
			comres = bma2x2_smbus_write_byte(client,
				BMA2X2_INT_SLO_NO_MOT_EN_SEL_INT__REG, &data1);
		}

	return comres;
	}


	comres = bma2x2_smbus_read_byte(client, BMA2X2_INT_ENABLE1_REG, &data1);
	comres = bma2x2_smbus_read_byte(client, BMA2X2_INT_ENABLE2_REG, &data2);

	value = value & 1;
	switch (InterruptType) {
	case 0:
		/* Low G Interrupt  */
		data2 = BMA2X2_SET_BITSLICE(data2, BMA2X2_EN_LOWG_INT, value);
		break;

	case 1:
		/* High G X Interrupt */
		data2 = BMA2X2_SET_BITSLICE(data2, BMA2X2_EN_HIGHG_X_INT,
				value);
		break;

	case 2:
		/* High G Y Interrupt */
		data2 = BMA2X2_SET_BITSLICE(data2, BMA2X2_EN_HIGHG_Y_INT,
				value);
		break;

	case 3:
		/* High G Z Interrupt */
		data2 = BMA2X2_SET_BITSLICE(data2, BMA2X2_EN_HIGHG_Z_INT,
				value);
		break;

	case 4:
		/* New Data Interrupt  */
		data2 = BMA2X2_SET_BITSLICE(data2, BMA2X2_EN_NEW_DATA_INT,
				value);
		break;

	case 5:
		/* Slope X Interrupt */
		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_SLOPE_X_INT,
				value);
		break;

	case 6:
		/* Slope Y Interrupt */
		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_SLOPE_Y_INT,
				value);
		break;

	case 7:
		/* Slope Z Interrupt */
		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_SLOPE_Z_INT,
				value);
		break;

	case 8:
		/* Single Tap Interrupt */
		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_SINGLE_TAP_INT,
				value);
		break;

	case 9:
		/* Double Tap Interrupt */
		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_DOUBLE_TAP_INT,
				value);
		break;

	case 10:
		/* Orient Interrupt  */
		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_ORIENT_INT, value);
		break;

	case 11:
		/* Flat Interrupt */
		data1 = BMA2X2_SET_BITSLICE(data1, BMA2X2_EN_FLAT_INT, value);
		break;

	default:
		break;
	}
	comres = bma2x2_smbus_write_byte(client, BMA2X2_INT_ENABLE1_REG,
			&data1);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_INT_ENABLE2_REG,
			&data2);

	return comres;
}


#if defined(BMA2X2_ENABLE_INT2)
static int bma2x2_get_interruptstatus1(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS1_REG, &data);
	*intstatus = data;

	return comres;
}

static int bma2x2_get_interruptstatus2(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS2_REG, &data);
	*intstatus = data;

	return comres;
}

static int bma2x2_get_HIGH_first(struct i2c_client *client, unsigned char
						param, unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	switch (param) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_ORIENT_HIGH_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_HIGHG_FIRST_X);
		*intstatus = data;
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_ORIENT_HIGH_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_HIGHG_FIRST_Y);
		*intstatus = data;
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_ORIENT_HIGH_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_HIGHG_FIRST_Z);
		*intstatus = data;
		break;
	default:
		break;
	}

	return comres;
}

static int bma2x2_get_HIGH_sign(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS_ORIENT_HIGH_REG,
			&data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_HIGHG_SIGN_S);
	*intstatus = data;

	return comres;
}

static int bma2x2_get_slope_first(struct i2c_client *client, unsigned char
	param, unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	switch (param) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_TAP_SLOPE_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_FIRST_X);
		*intstatus = data;
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_TAP_SLOPE_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_FIRST_Y);
		*intstatus = data;
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_STATUS_TAP_SLOPE_REG, &data);
		data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_FIRST_Z);
		*intstatus = data;
		break;
	default:
		break;
	}

	return comres;
}

static int bma2x2_get_slope_sign(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS_TAP_SLOPE_REG,
			&data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_SIGN_S);
	*intstatus = data;

	return comres;
}

static int bma2x2_get_orient_status(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS_ORIENT_HIGH_REG,
			&data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_ORIENT_S);
	*intstatus = data;

	return comres;
}

static int bma2x2_get_orient_flat_status(struct i2c_client *client, unsigned
		char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_STATUS_ORIENT_HIGH_REG,
			&data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_FLAT_S);
	*intstatus = data;

	return comres;
}
#endif /* defined(BMA2X2_ENABLE_INT1)||defined(BMA2X2_ENABLE_INT2) */

static int bma2x2_set_Int_Mode(struct i2c_client *client, unsigned char Mode)
{
	int comres = 0;
	unsigned char data = 0;


	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_INT_MODE_SEL__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_INT_MODE_SEL, Mode);
	comres = bma2x2_smbus_write_byte(client,
			BMA2X2_INT_MODE_SEL__REG, &data);


	return comres;
}

static int bma2x2_get_Int_Mode(struct i2c_client *client, unsigned char *Mode)
{
	int comres = 0;
	unsigned char data = 0;


	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_INT_MODE_SEL__REG, &data);
	data  = BMA2X2_GET_BITSLICE(data, BMA2X2_INT_MODE_SEL);
	*Mode = data;


	return comres;
}
static int bma2x2_set_slope_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data = 0;


	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_SLOPE_DUR__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_SLOPE_DUR, duration);
	comres = bma2x2_smbus_write_byte(client,
			BMA2X2_SLOPE_DUR__REG, &data);

	return comres;
}

static int bma2x2_get_slope_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;


	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_SLOPE_DURN_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLOPE_DUR);
	*status = data;


	return comres;
}

static int bma2x2_set_slope_no_mot_duration(struct i2c_client *client,
			unsigned char duration)
{
	int comres = 0;
	unsigned char data = 0;


	comres = bma2x2_smbus_read_byte(client,
			BMA2x2_SLO_NO_MOT_DUR__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2x2_SLO_NO_MOT_DUR, duration);
	comres = bma2x2_smbus_write_byte(client,
			BMA2x2_SLO_NO_MOT_DUR__REG, &data);


	return comres;
}

static int bma2x2_get_slope_no_mot_duration(struct i2c_client *client,
			unsigned char *status)
{
	int comres = 0;
	unsigned char data = 0;


	comres = bma2x2_smbus_read_byte(client,
			BMA2x2_SLO_NO_MOT_DUR__REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2x2_SLO_NO_MOT_DUR);
	*status = data;


	return comres;
}

static int bma2x2_set_slope_threshold(struct i2c_client *client,
		unsigned char threshold)
{
	int comres = 0;
	unsigned char data = 0;

	data = threshold;
	comres = bma2x2_smbus_write_byte(client,
			BMA2X2_SLOPE_THRES__REG, &data);

	return comres;
}

static int bma2x2_get_slope_threshold(struct i2c_client *client,
		unsigned char *status)
{
	int comres = 0;
	unsigned char data = 0;


	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_SLOPE_THRES_REG, &data);
	*status = data;

	return comres;
}

static int bma2x2_set_slope_no_mot_threshold(struct i2c_client *client,
		unsigned char threshold)
{
	int comres = 0;
	unsigned char data = 0;

	data = threshold;
	comres = bma2x2_smbus_write_byte(client,
			BMA2X2_SLO_NO_MOT_THRES_REG, &data);

	return comres;
}

static int bma2x2_get_slope_no_mot_threshold(struct i2c_client *client,
		unsigned char *status)
{
	int comres = 0;
	unsigned char data = 0;


	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_SLO_NO_MOT_THRES_REG, &data);
	*status = data;

	return comres;
}


static int bma2x2_set_low_g_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_LOWG_DUR__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_LOWG_DUR, duration);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_LOWG_DUR__REG, &data);

	return comres;
}

static int bma2x2_get_low_g_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_LOW_DURN_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_LOWG_DUR);
	*status = data;

	return comres;
}

static int bma2x2_set_low_g_threshold(struct i2c_client *client, unsigned char
		threshold)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_LOWG_THRES__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_LOWG_THRES, threshold);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_LOWG_THRES__REG, &data);

	return comres;
}

static int bma2x2_get_low_g_threshold(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_LOW_THRES_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_LOWG_THRES);
	*status = data;

	return comres;
}

static int bma2x2_set_high_g_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_HIGHG_DUR__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_HIGHG_DUR, duration);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_HIGHG_DUR__REG, &data);

	return comres;
}

static int bma2x2_get_high_g_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_HIGH_DURN_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_HIGHG_DUR);
	*status = data;

	return comres;
}

static int bma2x2_set_high_g_threshold(struct i2c_client *client, unsigned char
		threshold)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_HIGHG_THRES__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_HIGHG_THRES, threshold);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_HIGHG_THRES__REG,
			&data);

	return comres;
}

static int bma2x2_get_high_g_threshold(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_HIGH_THRES_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_HIGHG_THRES);
	*status = data;

	return comres;
}


static int bma2x2_set_tap_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_DUR__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_TAP_DUR, duration);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_TAP_DUR__REG, &data);

	return comres;
}

static int bma2x2_get_tap_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_PARAM_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_TAP_DUR);
	*status = data;

	return comres;
}

static int bma2x2_set_tap_shock(struct i2c_client *client, unsigned char setval)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_SHOCK_DURN__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_TAP_SHOCK_DURN, setval);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_TAP_SHOCK_DURN__REG,
			&data);

	return comres;
}

static int bma2x2_get_tap_shock(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_PARAM_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_TAP_SHOCK_DURN);
	*status = data;

	return comres;
}

static int bma2x2_set_tap_quiet(struct i2c_client *client, unsigned char
		duration)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_QUIET_DURN__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_TAP_QUIET_DURN, duration);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_TAP_QUIET_DURN__REG,
			&data);

	return comres;
}

static int bma2x2_get_tap_quiet(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_PARAM_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_TAP_QUIET_DURN);
	*status = data;

	return comres;
}

static int bma2x2_set_tap_threshold(struct i2c_client *client, unsigned char
		threshold)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_THRES__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_TAP_THRES, threshold);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_TAP_THRES__REG, &data);

	return comres;
}

static int bma2x2_get_tap_threshold(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_THRES_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_TAP_THRES);
	*status = data;

	return comres;
}

static int bma2x2_set_tap_samp(struct i2c_client *client, unsigned char samp)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_SAMPLES__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_TAP_SAMPLES, samp);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_TAP_SAMPLES__REG,
			&data);

	return comres;
}

static int bma2x2_get_tap_samp(struct i2c_client *client, unsigned char *status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TAP_THRES_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_TAP_SAMPLES);
	*status = data;

	return comres;
}

static int bma2x2_set_orient_mode(struct i2c_client *client, unsigned char mode)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_ORIENT_MODE__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_ORIENT_MODE, mode);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_ORIENT_MODE__REG,
			&data);

	return comres;
}

static int bma2x2_get_orient_mode(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_ORIENT_PARAM_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_ORIENT_MODE);
	*status = data;

	return comres;
}

static int bma2x2_set_orient_blocking(struct i2c_client *client, unsigned char
		samp)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_ORIENT_BLOCK__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_ORIENT_BLOCK, samp);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_ORIENT_BLOCK__REG,
			&data);

	return comres;
}

static int bma2x2_get_orient_blocking(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_ORIENT_PARAM_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_ORIENT_BLOCK);
	*status = data;

	return comres;
}

static int bma2x2_set_orient_hyst(struct i2c_client *client, unsigned char
		orienthyst)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_ORIENT_HYST__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_ORIENT_HYST, orienthyst);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_ORIENT_HYST__REG,
			&data);

	return comres;
}

static int bma2x2_get_orient_hyst(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_ORIENT_PARAM_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_ORIENT_HYST);
	*status = data;

	return comres;
}
static int bma2x2_set_theta_blocking(struct i2c_client *client, unsigned char
		thetablk)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_THETA_BLOCK__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_THETA_BLOCK, thetablk);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_THETA_BLOCK__REG,
			&data);

	return comres;
}

static int bma2x2_get_theta_blocking(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_THETA_BLOCK_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_THETA_BLOCK);
	*status = data;

	return comres;
}

static int bma2x2_set_theta_flat(struct i2c_client *client, unsigned char
		thetaflat)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_THETA_FLAT__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_THETA_FLAT, thetaflat);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_THETA_FLAT__REG, &data);

	return comres;
}

static int bma2x2_get_theta_flat(struct i2c_client *client, unsigned char
		*status)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_THETA_FLAT_REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_THETA_FLAT);
	*status = data;

	return comres;
}

static int bma2x2_set_flat_hold_time(struct i2c_client *client, unsigned char
		holdtime)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_FLAT_HOLD_TIME__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_FLAT_HOLD_TIME, holdtime);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_FLAT_HOLD_TIME__REG,
			&data);

	return comres;
}

static int bma2x2_get_flat_hold_time(struct i2c_client *client, unsigned char
		*holdtime)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_FLAT_HOLD_TIME_REG,
			&data);
	data  = BMA2X2_GET_BITSLICE(data, BMA2X2_FLAT_HOLD_TIME);
	*holdtime = data;

	return comres;
}

/*!
 * brief: bma2x2 switch from normal to suspend mode
 * @param[i] bma2x2
 * @param[i] data1, write to PMU_LPW
 * @param[i] data2, write to PMU_LOW_NOSIE
 *
 * @return zero success, none-zero failed
 */
static int bma2x2_normal_to_suspend(struct bma2x2_data *bma2x2,
				unsigned char data1, unsigned char data2)
{
	unsigned char current_fifo_mode;
	unsigned char current_op_mode;
	if (bma2x2 == NULL)
		return -1;
	/* get current op mode from mode register */
	if (bma2x2_get_mode(bma2x2->bma2x2_client, &current_op_mode) < 0)
		return -1;
	/* only aimed at operatiom mode chang from normal/lpw1 mode
	 * to suspend state.
	*/
	if (current_op_mode == BMA2X2_MODE_NORMAL ||
			current_op_mode == BMA2X2_MODE_LOWPOWER1) {
		/* get current fifo mode from fifo config register */
		if (bma2x2_get_fifo_mode(bma2x2->bma2x2_client,
							&current_fifo_mode) < 0)
			return -1;
		else {
			bma2x2_smbus_write_byte(bma2x2->bma2x2_client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
			bma2x2_smbus_write_byte(bma2x2->bma2x2_client,
					BMA2X2_MODE_CTRL_REG, &data1);
			/*! Aim at fifo workarounds with FIFO_CONFIG_1 */
			current_fifo_mode |= FIFO_WORKAROUNDS_MSK;
			bma2x2_smbus_write_byte(bma2x2->bma2x2_client,
				BMA2X2_FIFO_MODE__REG, &current_fifo_mode);
			mdelay(3);
			return 0;
		}
	} else {
		bma2x2_smbus_write_byte(bma2x2->bma2x2_client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
		bma2x2_smbus_write_byte(bma2x2->bma2x2_client,
					BMA2X2_MODE_CTRL_REG, &data1);
		mdelay(3);
		return 0;
	}

}

static int bma2x2_set_mode(struct i2c_client *client, unsigned char mode,
						unsigned char enabled_mode)
{
	int comres = 0;
	unsigned char data1 = 0;
	unsigned char data2 = 0;
	int ret = 0;
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	mutex_lock(&bma2x2->mode_mutex);
	if (BMA2X2_MODE_SUSPEND == mode) {
		if (enabled_mode != BMA_ENABLED_ALL) {
			if ((bma2x2->bma_mode_enabled &
						(1<<enabled_mode)) == 0) {
				/* sensor is already closed in this mode */
				mutex_unlock(&bma2x2->mode_mutex);
				return 0;
			} else {
				bma2x2->bma_mode_enabled &= ~(1<<enabled_mode);
			}
		} else {
			/* shut down, close all and force do it*/
			bma2x2->bma_mode_enabled = 0;
		}
	} else if (BMA2X2_MODE_NORMAL == mode) {
		if ((bma2x2->bma_mode_enabled & (1<<enabled_mode)) != 0) {
			/* sensor is already enabled in this mode */
			mutex_unlock(&bma2x2->mode_mutex);
			return 0;
		} else {
			bma2x2->bma_mode_enabled |= (1<<enabled_mode);
		}
	} else {
		/* other mode, close all and force do it*/
		bma2x2->bma_mode_enabled = 0;
	}
	mutex_unlock(&bma2x2->mode_mutex);

	if (mode < 6) {
		comres = bma2x2_smbus_read_byte(client, BMA2X2_MODE_CTRL_REG,
				&data1);
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_LOW_NOISE_CTRL_REG,
				&data2);
		switch (mode) {
		case BMA2X2_MODE_NORMAL:
				data1  = BMA2X2_SET_BITSLICE(data1,
						BMA2X2_MODE_CTRL, 0);
				data2  = BMA2X2_SET_BITSLICE(data2,
						BMA2X2_LOW_POWER_MODE, 0);
				bma2x2_smbus_write_byte(client,
						BMA2X2_MODE_CTRL_REG, &data1);
				mdelay(3);
				bma2x2_smbus_write_byte(client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
				break;
		case BMA2X2_MODE_LOWPOWER1:
				data1  = BMA2X2_SET_BITSLICE(data1,
						BMA2X2_MODE_CTRL, 2);
				data2  = BMA2X2_SET_BITSLICE(data2,
						BMA2X2_LOW_POWER_MODE, 0);
				bma2x2_smbus_write_byte(client,
						BMA2X2_MODE_CTRL_REG, &data1);
				mdelay(3);
				bma2x2_smbus_write_byte(client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
				break;
		case BMA2X2_MODE_SUSPEND:
			if (bma2x2->bma_mode_enabled != 0) {
				LOG_ERR("bma still working");
				return 0;
			}
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_MODE_CTRL, 4);
			data2  = BMA2X2_SET_BITSLICE(data2,
					BMA2X2_LOW_POWER_MODE, 0);
			/*aimed at anomaly resolution when switch to suspend*/
			ret = bma2x2_normal_to_suspend(bma2x2, data1, data2);
			if (ret < 0)
				LOG_ERR("Error switching to suspend");
			break;
		case BMA2X2_MODE_DEEP_SUSPEND:
			if (bma2x2->bma_mode_enabled != 0) {
				LOG_ERR("bma still working");
				return 0;
			}
			data1  = BMA2X2_SET_BITSLICE(data1,
				BMA2X2_MODE_CTRL, 1);
			data2  = BMA2X2_SET_BITSLICE(data2,
				BMA2X2_LOW_POWER_MODE, 1);
			bma2x2_smbus_write_byte(client,
				BMA2X2_MODE_CTRL_REG, &data1);
			mdelay(3);
			bma2x2_smbus_write_byte(client,
				BMA2X2_LOW_NOISE_CTRL_REG, &data2);
			break;
		case BMA2X2_MODE_LOWPOWER2:
				data1  = BMA2X2_SET_BITSLICE(data1,
						BMA2X2_MODE_CTRL, 2);
				data2  = BMA2X2_SET_BITSLICE(data2,
						BMA2X2_LOW_POWER_MODE, 1);
				bma2x2_smbus_write_byte(client,
						BMA2X2_MODE_CTRL_REG, &data1);
				mdelay(3);
				bma2x2_smbus_write_byte(client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
				break;
		case BMA2X2_MODE_STANDBY:
				data1  = BMA2X2_SET_BITSLICE(data1,
						BMA2X2_MODE_CTRL, 4);
				data2  = BMA2X2_SET_BITSLICE(data2,
						BMA2X2_LOW_POWER_MODE, 1);
				bma2x2_smbus_write_byte(client,
					BMA2X2_LOW_NOISE_CTRL_REG, &data2);
				mdelay(3);
				bma2x2_smbus_write_byte(client,
						BMA2X2_MODE_CTRL_REG, &data1);
				break;
		}
	} else {
		comres = -1;
	}

	return comres;
}


static int bma2x2_get_mode(struct i2c_client *client, unsigned char *mode)
{
	int comres = 0;
	unsigned char data1 = 0;
	unsigned char data2 = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_MODE_CTRL_REG, &data1);
	comres = bma2x2_smbus_read_byte(client, BMA2X2_LOW_NOISE_CTRL_REG,
			&data2);

	data1  = (data1 & 0xE0) >> 5;
	data2  = (data2 & 0x40) >> 6;


	if ((data1 == 0x00) && (data2 == 0x00)) {
		*mode  = BMA2X2_MODE_NORMAL;
	} else {
		if ((data1 == 0x02) && (data2 == 0x00)) {
			*mode  = BMA2X2_MODE_LOWPOWER1;
		} else {
			if ((data1 == 0x04 || data1 == 0x06) &&
						(data2 == 0x00)) {
				*mode  = BMA2X2_MODE_SUSPEND;
			} else {
				if (((data1 & 0x01) == 0x01)) {
					*mode  = BMA2X2_MODE_DEEP_SUSPEND;
				} else {
					if ((data1 == 0x02) &&
							(data2 == 0x01)) {
						*mode  = BMA2X2_MODE_LOWPOWER2;
					} else {
						if ((data1 == 0x04) && (data2 ==
									0x01)) {
							*mode  =
							BMA2X2_MODE_STANDBY;
						} else {
							*mode =
						BMA2X2_MODE_DEEP_SUSPEND;
						}
					}
				}
			}
		}
	}

	return comres;
}

static int bma2x2_set_range(struct i2c_client *client, unsigned char Range)
{
	int comres = 0;
	unsigned char data1 = 0;

	if ((Range == 3) || (Range == 5) || (Range == 8) || (Range == 12)) {
		comres = bma2x2_smbus_read_byte(client, BMA2X2_RANGE_SEL_REG,
				&data1);
		switch (Range) {
		case BMA2X2_RANGE_2G:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_RANGE_SEL, 3);
			break;
		case BMA2X2_RANGE_4G:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_RANGE_SEL, 5);
			break;
		case BMA2X2_RANGE_8G:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_RANGE_SEL, 8);
			break;
		case BMA2X2_RANGE_16G:
			data1  = BMA2X2_SET_BITSLICE(data1,
					BMA2X2_RANGE_SEL, 12);
			break;
		default:
			break;
		}
		comres += bma2x2_smbus_write_byte(client, BMA2X2_RANGE_SEL_REG,
				&data1);
	} else {
		comres = -1;
	}

	return comres;
}

static int bma2x2_get_range(struct i2c_client *client, unsigned char *Range)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_RANGE_SEL__REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_RANGE_SEL);
	*Range = data;

	return comres;
}


static int bma2x2_set_bandwidth(struct i2c_client *client, unsigned char BW)
{
	int comres = 0;
	unsigned char data = 0;
	int Bandwidth = 0;
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	LOG_INFO("bw=0x%02x\n", BW);
	if (BW > 7 && BW < 16) {
		switch (BW) {
		case BMA2X2_BW_7_81HZ:
			Bandwidth = BMA2X2_BW_7_81HZ;
			bma2x2->time_odr = 64000000;
			/*  7.81 Hz      64000 uS   */
			break;
		case BMA2X2_BW_15_63HZ:
			Bandwidth = BMA2X2_BW_15_63HZ;
			bma2x2->time_odr = 32000000;
			/*  15.63 Hz     32000 uS   */
			break;
		case BMA2X2_BW_31_25HZ:
			Bandwidth = BMA2X2_BW_31_25HZ;
			bma2x2->time_odr = 16000000;
			/*  31.25 Hz     16000 uS   */
			break;
		case BMA2X2_BW_62_50HZ:
			Bandwidth = BMA2X2_BW_62_50HZ;
			bma2x2->time_odr = 8000000;
			/*  62.50 Hz     8000 uS   */
			break;
		case BMA2X2_BW_125HZ:
			Bandwidth = BMA2X2_BW_125HZ;
			bma2x2->time_odr = 4000000;
			/*  125 Hz       4000 uS   */
			break;
		case BMA2X2_BW_250HZ:
			Bandwidth = BMA2X2_BW_250HZ;
			bma2x2->time_odr = 2000000;
			/*  250 Hz       2000 uS   */
			break;
		case BMA2X2_BW_500HZ:
			Bandwidth = BMA2X2_BW_500HZ;
			bma2x2->time_odr = 1000000;
			/*  500 Hz       1000 uS   */
			break;
		case BMA2X2_BW_1000HZ:
			Bandwidth = BMA2X2_BW_1000HZ;
			bma2x2->time_odr = 500000;
			/*  1000 Hz      500 uS   */
			break;
		default:
			break;
		}
		comres = bma2x2_smbus_read_byte(client, BMA2X2_BANDWIDTH__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_BANDWIDTH, Bandwidth);
		comres += bma2x2_smbus_write_byte(client, BMA2X2_BANDWIDTH__REG,
				&data);
	} else
		comres = -1;

	return comres;
}

static int bma2x2_get_bandwidth(struct i2c_client *client, unsigned char *BW)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_BANDWIDTH__REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_BANDWIDTH);
	*BW = data;

	return comres;
}

struct {
	unsigned int cutoff_ms;
	unsigned char mask;
} bma2x2_bandwidth_table[] = {
	{
	1, BMA2X2_BW_1000HZ}, {
	2, BMA2X2_BW_500HZ}, {
	5, BMA2X2_BW_250HZ}, {
	10, BMA2X2_BW_125HZ}, {
	15, BMA2X2_BW_62_50HZ}, { /* 4 */
	40, BMA2X2_BW_31_25HZ}, {
	100, BMA2X2_BW_15_63HZ}, {
	130, BMA2X2_BW_7_81HZ},};

int  bma2x2_update_odr(struct i2c_client *client, int poll_interval_ms)
{
	int err = -1;
	int i;
	int bw_ms;

	/* HZ: BW=ODR/2 */
	bw_ms = poll_interval_ms * 2;
	switch (poll_interval_ms) {
	case 200:
	    i = 7;
	    break;
	case 100:
	    i = 7;
	    break;
	case 66:
	    i = 7;
	    break;
	case 40:
	    i = 6;
	    break;
	case 20:
	    i = 5;
	    break;
	case 10:
	    i = 4;
	    break;
	case 8:
	    i = 4;
	    break;
	case 5:
	    i = 3;
	    break;
	default:
	    for (i = ARRAY_SIZE(bma2x2_bandwidth_table) - 1; i >= 0; i--) {
		if (bma2x2_bandwidth_table[i].cutoff_ms <= bw_ms)
			break;
	    }
	    break;
	}
	LOG_INFO("poll_interval_ms=%d,bw_ms=%d i=%d\n", poll_interval_ms, bw_ms, i);
	err = bma2x2_set_bandwidth(client, bma2x2_bandwidth_table[i].mask);
	if (poll_interval_ms >= 200)
		odr_delay = 1;
	else if (poll_interval_ms >= 100)
		odr_delay = 2;
	else
		odr_delay = 3;
	LOG_INFO("bma2x2 poll_interval_ms=%d err=%d\n", poll_interval_ms, err);
	return err;
}
int bma2x2_get_sleep_duration(struct i2c_client *client, unsigned char
		*sleep_dur)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_SLEEP_DUR__REG, &data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_SLEEP_DUR);
	*sleep_dur = data;

	return comres;
}

int bma2x2_set_sleep_duration(struct i2c_client *client, unsigned char
		sleep_dur)
{
	int comres = 0;
	unsigned char data = 0;
	int sleep_duration = 0;

	if (sleep_dur > 4 && sleep_dur < 16) {
		switch (sleep_dur) {
		case BMA2X2_SLEEP_DUR_0_5MS:
			sleep_duration = BMA2X2_SLEEP_DUR_0_5MS;

			/*  0.5 MS   */
			break;
		case BMA2X2_SLEEP_DUR_1MS:
			sleep_duration = BMA2X2_SLEEP_DUR_1MS;

			/*  1 MS  */
			break;
		case BMA2X2_SLEEP_DUR_2MS:
			sleep_duration = BMA2X2_SLEEP_DUR_2MS;

			/*  2 MS  */
			break;
		case BMA2X2_SLEEP_DUR_4MS:
			sleep_duration = BMA2X2_SLEEP_DUR_4MS;

			/*  4 MS   */
			break;
		case BMA2X2_SLEEP_DUR_6MS:
			sleep_duration = BMA2X2_SLEEP_DUR_6MS;

			/*  6 MS  */
			break;
		case BMA2X2_SLEEP_DUR_10MS:
			sleep_duration = BMA2X2_SLEEP_DUR_10MS;

			/*  10 MS  */
			break;
		case BMA2X2_SLEEP_DUR_25MS:
			sleep_duration = BMA2X2_SLEEP_DUR_25MS;

			/*  25 MS  */
			break;
		case BMA2X2_SLEEP_DUR_50MS:
			sleep_duration = BMA2X2_SLEEP_DUR_50MS;

			/*  50 MS   */
			break;
		case BMA2X2_SLEEP_DUR_100MS:
			sleep_duration = BMA2X2_SLEEP_DUR_100MS;

			/*  100 MS  */
			break;
		case BMA2X2_SLEEP_DUR_500MS:
			sleep_duration = BMA2X2_SLEEP_DUR_500MS;

			/*  500 MS   */
			break;
		case BMA2X2_SLEEP_DUR_1S:
			sleep_duration = BMA2X2_SLEEP_DUR_1S;

			/*  1 SECS   */
			break;
		default:
			break;
		}
		comres = bma2x2_smbus_read_byte(client, BMA2X2_SLEEP_DUR__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_SLEEP_DUR,
				sleep_duration);
		comres = bma2x2_smbus_write_byte(client, BMA2X2_SLEEP_DUR__REG,
				&data);
	} else {
		comres = -1;
	}


	return comres;
}

static int bma2x2_get_fifo_mode(struct i2c_client *client, unsigned char
		*fifo_mode)
{
	int comres;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_FIFO_MODE__REG, &data);
	*fifo_mode = BMA2X2_GET_BITSLICE(data, BMA2X2_FIFO_MODE);

	return comres;
}

static int bma2x2_set_fifo_mode(struct i2c_client *client, unsigned char
		fifo_mode)
{
	unsigned char data = 0;
	int comres = 0;

	if (fifo_mode < 4) {
		comres = bma2x2_smbus_read_byte(client, BMA2X2_FIFO_MODE__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_FIFO_MODE, fifo_mode);
		/*! Aim at fifo workarounds with FIFO_CONFIG_1 */
		data |= FIFO_WORKAROUNDS_MSK;
		comres = bma2x2_smbus_write_byte(client, BMA2X2_FIFO_MODE__REG,
				&data);
	} else {
		comres = -1;
	}

	return comres;
}

static int bma2x2_get_fifo_trig(struct i2c_client *client, unsigned char
		*fifo_trig)
{
	int comres;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_FIFO_TRIGGER_ACTION__REG, &data);
	*fifo_trig = BMA2X2_GET_BITSLICE(data, BMA2X2_FIFO_TRIGGER_ACTION);

	return comres;
}

static int bma2x2_set_fifo_trig(struct i2c_client *client, unsigned char
		fifo_trig)
{
	unsigned char data = 0;
	int comres = 0;

	if (fifo_trig < 4) {
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_FIFO_TRIGGER_ACTION__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_FIFO_TRIGGER_ACTION,
				fifo_trig);
		/*! Aim at fifo workarounds with FIFO_CONFIG_1 */
		data |= FIFO_WORKAROUNDS_MSK;
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_FIFO_TRIGGER_ACTION__REG, &data);
	} else {
		comres = -1;
	}

	return comres;
}

static int bma2x2_get_fifo_trig_src(struct i2c_client *client, unsigned char
		*trig_src)
{
	int comres;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_FIFO_TRIGGER_SOURCE__REG, &data);
	*trig_src = BMA2X2_GET_BITSLICE(data, BMA2X2_FIFO_TRIGGER_SOURCE);

	return comres;
}

static int bma2x2_set_fifo_trig_src(struct i2c_client *client, unsigned char
		trig_src)
{
	unsigned char data = 0;
	int comres = 0;

	if (trig_src < 4) {
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_FIFO_TRIGGER_SOURCE__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_FIFO_TRIGGER_SOURCE,
				trig_src);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_FIFO_TRIGGER_SOURCE__REG, &data);
	} else {
		comres = -1;
	}

	return comres;
}

static int bma2x2_get_fifo_framecount(struct i2c_client *client, unsigned char
			 *framecount)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_FIFO_FRAME_COUNTER_S__REG, &data);
	*framecount = BMA2X2_GET_BITSLICE(data, BMA2X2_FIFO_FRAME_COUNTER_S);

	return comres;
}

static int bma2x2_get_fifo_data_sel(struct i2c_client *client, unsigned char
		*data_sel)
{
	int comres;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client,
			BMA2X2_FIFO_DATA_SELECT__REG, &data);
	*data_sel = BMA2X2_GET_BITSLICE(data, BMA2X2_FIFO_DATA_SELECT);

	return comres;
}

static int bma2x2_set_fifo_data_sel(struct i2c_client *client, unsigned char
		data_sel)
{
	unsigned char data = 0;
	int comres = 0;

	if (data_sel < 4) {
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_FIFO_DATA_SELECT__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_FIFO_DATA_SELECT,
				data_sel);
		/*! Aim at fifo workarounds with FIFO_CONFIG_1 */
		data |= FIFO_WORKAROUNDS_MSK;
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_FIFO_DATA_SELECT__REG,
				&data);
	} else {
		comres = -1;
	}

	return comres;
}

static int bma2x2_get_offset_target(struct i2c_client *client, unsigned char
		channel, unsigned char *offset)
{
	unsigned char data = 0;
	int comres = 0;

	switch (channel) {
	case BMA2X2_CUT_OFF:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_COMP_CUTOFF__REG, &data);
		*offset = BMA2X2_GET_BITSLICE(data, BMA2X2_COMP_CUTOFF);
		break;
	case BMA2X2_OFFSET_TRIGGER_X:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_COMP_TARGET_OFFSET_X__REG, &data);
		*offset = BMA2X2_GET_BITSLICE(data,
				BMA2X2_COMP_TARGET_OFFSET_X);
		break;
	case BMA2X2_OFFSET_TRIGGER_Y:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_COMP_TARGET_OFFSET_Y__REG, &data);
		*offset = BMA2X2_GET_BITSLICE(data,
				BMA2X2_COMP_TARGET_OFFSET_Y);
		break;
	case BMA2X2_OFFSET_TRIGGER_Z:
		comres = bma2x2_smbus_read_byte(client,
			BMA2X2_COMP_TARGET_OFFSET_Z__REG, &data);
		*offset = BMA2X2_GET_BITSLICE(data,
				BMA2X2_COMP_TARGET_OFFSET_Z);
		break;
	default:
		comres = -1;
		break;
	}

	return comres;
}

static int bma2x2_set_offset_target(struct i2c_client *client, unsigned char
		channel, unsigned char offset)
{
	unsigned char data = 0;
	int comres = 0;

	switch (channel) {
	case BMA2X2_CUT_OFF:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_COMP_CUTOFF__REG, &data);
		data = BMA2X2_SET_BITSLICE(data, BMA2X2_COMP_CUTOFF,
				offset);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_COMP_CUTOFF__REG, &data);
		break;
	case BMA2X2_OFFSET_TRIGGER_X:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_COMP_TARGET_OFFSET_X__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data,
				BMA2X2_COMP_TARGET_OFFSET_X,
				offset);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_COMP_TARGET_OFFSET_X__REG,
				&data);
		break;
	case BMA2X2_OFFSET_TRIGGER_Y:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_COMP_TARGET_OFFSET_Y__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data,
				BMA2X2_COMP_TARGET_OFFSET_Y,
				offset);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_COMP_TARGET_OFFSET_Y__REG,
				&data);
		break;
	case BMA2X2_OFFSET_TRIGGER_Z:
		comres = bma2x2_smbus_read_byte(client,
				BMA2X2_COMP_TARGET_OFFSET_Z__REG,
				&data);
		data = BMA2X2_SET_BITSLICE(data,
				BMA2X2_COMP_TARGET_OFFSET_Z,
				offset);
		comres = bma2x2_smbus_write_byte(client,
				BMA2X2_COMP_TARGET_OFFSET_Z__REG,
				&data);
		break;
	default:
		comres = -1;
		break;
	}

	return comres;
}

static int bma2x2_get_cal_ready(struct i2c_client *client,
					unsigned char *calrdy)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_FAST_CAL_RDY_S__REG,
			&data);
	data = BMA2X2_GET_BITSLICE(data, BMA2X2_FAST_CAL_RDY_S);
	*calrdy = data;

	return comres;
}

static int bma2x2_set_cal_trigger(struct i2c_client *client, unsigned char
		caltrigger)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_CAL_TRIGGER__REG, &data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_CAL_TRIGGER, caltrigger);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_CAL_TRIGGER__REG,
			&data);

	return comres;
}

static int bma2x2_write_reg(struct i2c_client *client, unsigned char addr,
		unsigned char *data)
{
	int comres = 0;
	comres = bma2x2_smbus_write_byte(client, addr, data);

	return comres;
}

static int bma2x2_set_offset_x(struct i2c_client *client, unsigned char
		offsetfilt)
{
	int comres = 0;
	unsigned char data = 0;

	data =  offsetfilt;

#ifdef CONFIG_SENSORS_BMI058
	comres = bma2x2_smbus_write_byte(client, BMI058_OFFSET_X_AXIS_REG,
							&data);
#else
	comres = bma2x2_smbus_write_byte(client, BMA2X2_OFFSET_X_AXIS_REG,
						&data);
#endif

	return comres;
}

static int bma2x2_get_offset_x(struct i2c_client *client, unsigned char
						*offsetfilt)
{
	int comres = 0;
	unsigned char data = 0;

#ifdef CONFIG_SENSORS_BMI058
	comres = bma2x2_smbus_read_byte(client, BMI058_OFFSET_X_AXIS_REG,
							&data);
#else
	comres = bma2x2_smbus_read_byte(client, BMA2X2_OFFSET_X_AXIS_REG,
							&data);
#endif
	*offsetfilt = data;

	return comres;
}

static int bma2x2_set_offset_y(struct i2c_client *client, unsigned char
						offsetfilt)
{
	int comres = 0;
	unsigned char data = 0;

	data =  offsetfilt;

#ifdef CONFIG_SENSORS_BMI058
	comres = bma2x2_smbus_write_byte(client, BMI058_OFFSET_Y_AXIS_REG,
							&data);
#else
	comres = bma2x2_smbus_write_byte(client, BMA2X2_OFFSET_Y_AXIS_REG,
							&data);
#endif
	return comres;
}

static int bma2x2_get_offset_y(struct i2c_client *client, unsigned char
						*offsetfilt)
{
	int comres = 0;
	unsigned char data = 0;

#ifdef CONFIG_SENSORS_BMI058
	comres = bma2x2_smbus_read_byte(client, BMI058_OFFSET_Y_AXIS_REG,
							&data);
#else
	comres = bma2x2_smbus_read_byte(client, BMA2X2_OFFSET_Y_AXIS_REG,
							&data);
#endif
	*offsetfilt = data;

	return comres;
}

static int bma2x2_set_offset_z(struct i2c_client *client, unsigned char
						offsetfilt)
{
	int comres = 0;
	unsigned char data = 0;

	data =  offsetfilt;
	comres = bma2x2_smbus_write_byte(client, BMA2X2_OFFSET_Z_AXIS_REG,
						&data);

	return comres;
}

static int bma2x2_get_offset_z(struct i2c_client *client, unsigned char
						*offsetfilt)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_OFFSET_Z_AXIS_REG,
						&data);
	*offsetfilt = data;

	return comres;
}


static int bma2x2_set_selftest_st(struct i2c_client *client, unsigned char
		selftest)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_EN_SELF_TEST__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_EN_SELF_TEST, selftest);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_EN_SELF_TEST__REG,
			&data);

	return comres;
}

static int bma2x2_set_selftest_stn(struct i2c_client *client, unsigned char stn)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_NEG_SELF_TEST__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_NEG_SELF_TEST, stn);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_NEG_SELF_TEST__REG,
			&data);

	return comres;
}

static int bma2x2_set_selftest_amp(struct i2c_client *client, unsigned char amp)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_SELF_TEST_AMP__REG,
			&data);
	data = BMA2X2_SET_BITSLICE(data, BMA2X2_SELF_TEST_AMP, amp);
	comres = bma2x2_smbus_write_byte(client, BMA2X2_SELF_TEST_AMP__REG,
			&data);

	return comres;
}

static int bma2x2_read_accel_x(struct i2c_client *client,
				signed char sensor_type, short *a_x)
{
	int comres = 0;
	unsigned char data[2];

	switch (sensor_type) {
	case 0:
		comres = bma2x2_smbus_read_byte_block(client,
					BMA2X2_ACC_X12_LSB__REG, data, 2);
		*a_x = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_X12_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_X_MSB)<<(BMA2X2_ACC_X12_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(BMA2X2_ACC_X12_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(BMA2X2_ACC_X12_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		break;
	case 1:
		comres = bma2x2_smbus_read_byte_block(client,
					BMA2X2_ACC_X10_LSB__REG, data, 2);
		*a_x = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_X10_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_X_MSB)<<(BMA2X2_ACC_X10_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(BMA2X2_ACC_X10_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(BMA2X2_ACC_X10_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		break;
	case 2:
		comres = bma2x2_smbus_read_byte_block(client,
					BMA2X2_ACC_X8_LSB__REG, data, 2);
		*a_x = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_X8_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_X_MSB)<<(BMA2X2_ACC_X8_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(BMA2X2_ACC_X8_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(BMA2X2_ACC_X8_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		break;
	case 3:
		comres = bma2x2_smbus_read_byte_block(client,
					BMA2X2_ACC_X14_LSB__REG, data, 2);
		*a_x = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_X14_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_X_MSB)<<(BMA2X2_ACC_X14_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(BMA2X2_ACC_X14_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(BMA2X2_ACC_X14_LSB__LEN
					+ BMA2X2_ACC_X_MSB__LEN));
		break;
	default:
		break;
	}

	return comres;
}

static int bma2x2_soft_reset(struct i2c_client *client)
{
	int comres = 0;
	unsigned char data = BMA2X2_EN_SOFT_RESET_VALUE;

	comres = bma2x2_smbus_write_byte(client, BMA2X2_EN_SOFT_RESET__REG,
					&data);

	return comres;
}

static int bma2x2_read_accel_y(struct i2c_client *client,
				signed char sensor_type, short *a_y)
{
	int comres = 0;
	unsigned char data[2];

	switch (sensor_type) {
	case 0:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Y12_LSB__REG, data, 2);
		*a_y = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Y12_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Y_MSB)<<(BMA2X2_ACC_Y12_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(BMA2X2_ACC_Y12_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(BMA2X2_ACC_Y12_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		break;
	case 1:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Y10_LSB__REG, data, 2);
		*a_y = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Y10_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Y_MSB)<<(BMA2X2_ACC_Y10_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(BMA2X2_ACC_Y10_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(BMA2X2_ACC_Y10_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		break;
	case 2:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Y8_LSB__REG, data, 2);
		*a_y = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Y8_LSB)|
				(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Y_MSB)<<(BMA2X2_ACC_Y8_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(BMA2X2_ACC_Y8_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(BMA2X2_ACC_Y8_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		break;
	case 3:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Y14_LSB__REG, data, 2);
		*a_y = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Y14_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Y_MSB)<<(BMA2X2_ACC_Y14_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(BMA2X2_ACC_Y14_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(BMA2X2_ACC_Y14_LSB__LEN
						+ BMA2X2_ACC_Y_MSB__LEN));
		break;
	default:
		break;
	}

	return comres;
}

static int bma2x2_read_accel_z(struct i2c_client *client,
				signed char sensor_type, short *a_z)
{
	int comres = 0;
	unsigned char data[2];

	switch (sensor_type) {
	case 0:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Z12_LSB__REG, data, 2);
		*a_z = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Z12_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Z_MSB)<<(BMA2X2_ACC_Z12_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(BMA2X2_ACC_Z12_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(BMA2X2_ACC_Z12_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		break;
	case 1:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Z10_LSB__REG, data, 2);
		*a_z = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Z10_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Z_MSB)<<(BMA2X2_ACC_Z10_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(BMA2X2_ACC_Z10_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(BMA2X2_ACC_Z10_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		break;
	case 2:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Z8_LSB__REG, data, 2);
		*a_z = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Z8_LSB)|
			(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Z_MSB)<<(BMA2X2_ACC_Z8_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(BMA2X2_ACC_Z8_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(BMA2X2_ACC_Z8_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		break;
	case 3:
		comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_Z14_LSB__REG, data, 2);
		*a_z = BMA2X2_GET_BITSLICE(data[0], BMA2X2_ACC_Z14_LSB)|
				(BMA2X2_GET_BITSLICE(data[1],
				BMA2X2_ACC_Z_MSB)<<(BMA2X2_ACC_Z14_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(BMA2X2_ACC_Z14_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(BMA2X2_ACC_Z14_LSB__LEN
						+ BMA2X2_ACC_Z_MSB__LEN));
		break;
	default:
		break;
	}

	return comres;
}

static int bma2x2_read_temperature(struct i2c_client *client,
					signed char *temperature)
{
	unsigned char data = 0;
	int comres = 0;

	comres = bma2x2_smbus_read_byte(client, BMA2X2_TEMPERATURE_REG, &data);
	*temperature = (signed char)data;

	return comres;
}

static ssize_t bma2x2_enable_int_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int type, value;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
#ifdef CONFIG_SENSORS_BMI058
	int i;
#endif

	sscanf(buf, "%3d %3d", &type, &value);

#ifdef CONFIG_SENSORS_BMI058
	for (i = 0; i < sizeof(int_map) / sizeof(struct interrupt_map_t); i++) {
		if (int_map[i].x == type) {
			type = int_map[i].y;
			break;
		}
		if (int_map[i].y == type) {
			type = int_map[i].x;
			break;
		}
	}
#endif

	if (bma2x2_set_Int_Enable(bma2x2->bma2x2_client, type, value) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_int_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_Int_Mode(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_int_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_Int_Mode(bma2x2->bma2x2_client, (unsigned char)data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_slope_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_slope_duration(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_slope_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_slope_duration(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_slope_no_mot_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_slope_no_mot_duration(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_slope_no_mot_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_slope_no_mot_duration(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_slope_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_slope_threshold(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_slope_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_slope_threshold(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_slope_no_mot_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_slope_no_mot_threshold(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_slope_no_mot_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_slope_no_mot_threshold(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_high_g_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_high_g_duration(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_high_g_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_high_g_duration(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_high_g_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_high_g_threshold(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_high_g_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_high_g_threshold(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_low_g_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_low_g_duration(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_low_g_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_low_g_duration(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_low_g_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_low_g_threshold(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_low_g_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_low_g_threshold(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_tap_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_threshold(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_tap_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_tap_threshold(bma2x2->bma2x2_client, (unsigned char)data)
			< 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_tap_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_duration(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_tap_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_tap_duration(bma2x2->bma2x2_client, (unsigned char)data)
			< 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_tap_quiet_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_quiet(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_tap_quiet_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_tap_quiet(bma2x2->bma2x2_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_tap_shock_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_shock(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_tap_shock_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_tap_shock(bma2x2->bma2x2_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_tap_samp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_samp(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_tap_samp_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_tap_samp(bma2x2->bma2x2_client, (unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_orient_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_orient_mode(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_orient_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_orient_mode(bma2x2->bma2x2_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_orient_blocking_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_orient_blocking(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_orient_blocking_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_orient_blocking(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_orient_hyst_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_orient_hyst(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_orient_hyst_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_orient_hyst(bma2x2->bma2x2_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_orient_theta_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_theta_blocking(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_orient_theta_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_theta_blocking(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_flat_theta_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_theta_flat(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_flat_theta_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_theta_flat(bma2x2->bma2x2_client, (unsigned char)data) <
			0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_flat_hold_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_flat_hold_time(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&bma2x2->selftest_result));

}

static ssize_t bma2x2_softreset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_soft_reset(bma2x2->bma2x2_client) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_selftest_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{

	unsigned long data;
	unsigned char clear_value = 0;
	int error;
	short value1 = 0;
	short value2 = 0;
	short diff = 0;
	unsigned long result = 0;
	unsigned char test_result_branch = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	bma2x2_soft_reset(bma2x2->bma2x2_client);
	mdelay(5);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data != 1)
		return -EINVAL;

	bma2x2_write_reg(bma2x2->bma2x2_client, 0x32, &clear_value);

	if ((bma2x2->sensor_type == BMA280_TYPE) ||
		(bma2x2->sensor_type == BMA255_TYPE)) {
#ifdef CONFIG_SENSORS_BMI058
		/*set self test amp */
		if (bma2x2_set_selftest_amp(bma2x2->bma2x2_client, 1) < 0)
			return -EINVAL;
		/* set to 8 G range */
		if (bma2x2_set_range(bma2x2->bma2x2_client,
							BMA2X2_RANGE_8G) < 0)
			return -EINVAL;
#else
		/* set to 4 G range */
		if (bma2x2_set_range(bma2x2->bma2x2_client,
							BMA2X2_RANGE_4G) < 0)
			return -EINVAL;
#endif
	}

	if ((bma2x2->sensor_type == BMA250E_TYPE) ||
			(bma2x2->sensor_type == BMA222E_TYPE)) {
		/* set to 8 G range */
		if (bma2x2_set_range(bma2x2->bma2x2_client, 8) < 0)
			return -EINVAL;
		if (bma2x2_set_selftest_amp(bma2x2->bma2x2_client, 1) < 0)
			return -EINVAL;
	}

	/* 1 for x-axis(but BMI058 is 1 for y-axis )*/
	bma2x2_set_selftest_st(bma2x2->bma2x2_client, 1);
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0);
	mdelay(10);
	bma2x2_read_accel_x(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &value1);
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1);
	mdelay(10);
	bma2x2_read_accel_x(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &value2);
	diff = value1-value2;

#ifdef CONFIG_SENSORS_BMI058
	LOG_INFO("diff y is %d,value1 is %d, value2 is %d\n", diff,
				value1, value2);
	test_result_branch = 2;
#else
	LOG_INFO("diff x is %d,value1 is %d, value2 is %d\n", diff,
				value1, value2);
	test_result_branch = 1;
#endif

	if (bma2x2->sensor_type == BMA280_TYPE) {
#ifdef CONFIG_SENSORS_BMI058
		if (abs(diff) < 819)
			result |= test_result_branch;
#else
		if (abs(diff) < 1638)
			result |= test_result_branch;
#endif
	}
	if (bma2x2->sensor_type == BMA255_TYPE) {
		if (abs(diff) < 409)
			result |= 1;
	}
	if (bma2x2->sensor_type == BMA250E_TYPE) {
		if (abs(diff) < 51)
			result |= 1;
	}
	if (bma2x2->sensor_type == BMA222E_TYPE) {
		if (abs(diff) < 12)
			result |= 1;
	}

	/* 2 for y-axis but BMI058 is 1*/
	bma2x2_set_selftest_st(bma2x2->bma2x2_client, 2);
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0);
	mdelay(10);
	bma2x2_read_accel_y(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &value1);
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1);
	mdelay(10);
	bma2x2_read_accel_y(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &value2);
	diff = value1-value2;

#ifdef CONFIG_SENSORS_BMI058
	LOG_INFO("diff x is %d,value1 is %d, value2 is %d\n", diff,
				value1, value2);
	test_result_branch = 1;
#else
	LOG_INFO("diff y is %d,value1 is %d, value2 is %d\n", diff,
				value1, value2);
	test_result_branch = 2;
#endif

	if (bma2x2->sensor_type == BMA280_TYPE) {
#ifdef CONFIG_SENSORS_BMI058
		if (abs(diff) < 819)
			result |= test_result_branch;
#else
		if (abs(diff) < 1638)
			result |= test_result_branch;
#endif
	}
	if (bma2x2->sensor_type == BMA255_TYPE) {
		if (abs(diff) < 409)
			result |= test_result_branch;
	}
	if (bma2x2->sensor_type == BMA250E_TYPE) {
		if (abs(diff) < 51)
			result |= test_result_branch;
	}
	if (bma2x2->sensor_type == BMA222E_TYPE) {
		if (abs(diff) < 12)
			result |= test_result_branch;
	}


	bma2x2_set_selftest_st(bma2x2->bma2x2_client, 3); /* 3 for z-axis*/
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0);
	mdelay(10);
	bma2x2_read_accel_z(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &value1);
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1);
	mdelay(10);
	bma2x2_read_accel_z(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &value2);
	diff = value1-value2;

	LOG_INFO("diff z is %d,value1 is %d, value2 is %d\n", diff,
			value1, value2);

	if (bma2x2->sensor_type == BMA280_TYPE) {
#ifdef CONFIG_SENSORS_BMI058
			if (abs(diff) < 409)
				result |= 4;
#else
			if (abs(diff) < 819)
				result |= 4;
#endif
	}
	if (bma2x2->sensor_type == BMA255_TYPE) {
		if (abs(diff) < 204)
			result |= 4;
	}
	if (bma2x2->sensor_type == BMA250E_TYPE) {
		if (abs(diff) < 25)
			result |= 4;
	}
	if (bma2x2->sensor_type == BMA222E_TYPE) {
		if (abs(diff) < 6)
			result |= 4;
	}

	/* self test for bma254 */
	if ((bma2x2->sensor_type == BMA255_TYPE) && (result > 0)) {
		result = 0;
		bma2x2_soft_reset(bma2x2->bma2x2_client);
		mdelay(5);
		bma2x2_write_reg(bma2x2->bma2x2_client, 0x32, &clear_value);
		/* set to 8 G range */
		if (bma2x2_set_range(bma2x2->bma2x2_client, 8) < 0)
			return -EINVAL;
		if (bma2x2_set_selftest_amp(bma2x2->bma2x2_client, 1) < 0)
			return -EINVAL;

		bma2x2_set_selftest_st(bma2x2->bma2x2_client, 1); /* 1
								for x-axis*/
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0); /*
							positive direction*/
		mdelay(10);
		bma2x2_read_accel_x(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value1);
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1); /*
							negative direction*/
		mdelay(10);
		bma2x2_read_accel_x(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value2);
		diff = value1-value2;

		LOG_INFO("diff x is %d,value1 is %d, value2 is %d\n",
						diff, value1, value2);
		if (abs(diff) < 204)
			result |= 1;

		bma2x2_set_selftest_st(bma2x2->bma2x2_client, 2); /* 2
								for y-axis*/
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0); /*
							positive direction*/
		mdelay(10);
		bma2x2_read_accel_y(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value1);
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1); /*
							negative direction*/
		mdelay(10);
		bma2x2_read_accel_y(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value2);
		diff = value1-value2;
		LOG_INFO("diff y is %d,value1 is %d, value2 is %d\n",
						diff, value1, value2);

		if (abs(diff) < 204)
			result |= 2;

		bma2x2_set_selftest_st(bma2x2->bma2x2_client, 3); /* 3
								for z-axis*/
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0); /*
							positive direction*/
		mdelay(10);
		bma2x2_read_accel_z(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value1);
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1); /*
							negative direction*/
		mdelay(10);
		bma2x2_read_accel_z(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value2);
		diff = value1-value2;

		LOG_INFO("diff z is %d,value1 is %d, value2 is %d\n",
						diff, value1, value2);
		if (abs(diff) < 102)
			result |= 4;
	}

	atomic_set(&bma2x2->selftest_result, (unsigned int)result);

	bma2x2_soft_reset(bma2x2->bma2x2_client);
	mdelay(5);
	LOG_INFO("self test finished\n");

	return count;
}



static ssize_t bma2x2_flat_hold_time_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_flat_hold_time(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

const int bma2x2_sensor_bitwidth[] = {
	12,  10,  8, 14
};

static int bma2x2_read_accel_xyz(struct i2c_client *client,
		signed char sensor_type, struct bma2x2acc *acc)
{
	int comres = -1;
	unsigned char data[6];

#ifndef BMA2X2_SENSOR_IDENTIFICATION_ENABLE
	int bitwidth;
#endif
	comres = bma2x2_smbus_read_byte_block(client,
				BMA2X2_ACC_X12_LSB__REG, data, 6);
	if (comres)
		return comres;
	if (sensor_type >= 4)
		return -EINVAL;

	acc->x = (data[1]<<8)|data[0];
	acc->y = (data[3]<<8)|data[2];
	acc->z = (data[5]<<8)|data[4];
#if 0
	if (!test_bit(0, (unsigned long *)data))
	    LOG_INFO("x NOT new data\n");
	if (!test_bit(0, (unsigned long *)data + 2))
	    LOG_INFO("y NOT new data\n");
	if (!test_bit(0, (unsigned long *)data + 4))
	    LOG_INFO("z NOT new data\n");
#endif
#ifndef BMA2X2_SENSOR_IDENTIFICATION_ENABLE
	bitwidth = bma2x2_sensor_bitwidth[sensor_type];
	acc->x = (acc->x >> (16 - bitwidth));
	acc->y = (acc->y >> (16 - bitwidth));
	acc->z = (acc->z >> (16 - bitwidth));
#endif
	return comres;
}

#ifndef BMA_ENABLE_NEWDATA_INT
static void bma2x2_work_func(struct work_struct *work)
{
	int rd_result = -1;

	struct bma2x2_data *bma2x2 = container_of((struct delayed_work *)work,
			struct bma2x2_data, work);
	static struct bma2x2acc acc;
	unsigned long delay = msecs_to_jiffies(atomic_read(&bma2x2->delay));

	rd_result = bma2x2_read_accel_xyz(bma2x2->bma2x2_client,
			bma2x2->sensor_type, &acc);
	input_report_abs(bma2x2->input, ABS_X, acc.x);
	input_report_abs(bma2x2->input, ABS_Y, acc.y);
	input_report_abs(bma2x2->input, ABS_Z, acc.z);
	input_sync(bma2x2->input);
	mutex_lock(&bma2x2->value_mutex);
	bma2x2->value = acc;
	mutex_unlock(&bma2x2->value_mutex);
	schedule_delayed_work(&bma2x2->work, delay);
}
#endif
static struct workqueue_struct *reportdata_wq;

uint64_t bma2x2_get_alarm_timestamp(void)
{
	uint64_t ts_ap;
	struct timespec64 tmp_time;
	ktime_get_boottime_ts64(&tmp_time);
	ts_ap = (uint64_t)tmp_time.tv_sec * 1000000000 + tmp_time.tv_nsec;
	return ts_ap;
}

#define ABS(x) ((x) > 0 ? (x) : -(x))
static int bma2x2_ring_buf_put(bma2x2_buf_t data);


static void bma2x2_timer_work_fun(struct work_struct *work)
{
	struct  bma2x2_data *bma2x2 =
		container_of(work,
				struct bma2x2_data, report_data_work);
	int i;
	unsigned char count = 0;
	unsigned char mode = 0;
	signed char fifo_data_out[MAX_FIFO_F_LEVEL * MAX_FIFO_F_BYTES] = {0};
	unsigned char f_len = 0;
	uint64_t del = 0;
	uint64_t time_internal = 0;
	int64_t drift_time = 0;
	static uint64_t time_odr;
	struct bma2x2acc acc_lsb;
	bma2x2_buf_t acc_data;
	struct timespec ts;
	static uint32_t data_cnt;
	static uint32_t pre_data_cnt;
	static int64_t sample_drift_offset;

	LOG_INFO("entry\n");
	if (bma2x2->fifo_datasel) {
		/*Select one axis data output for every fifo frame*/
		f_len = 2;
	} else	{
		/*Select X Y Z axis data output for every fifo frame*/
		f_len = 6;
	}
	if (bma2x2_get_fifo_framecount(bma2x2->bma2x2_client, &count) < 0) {
		LOG_ERR("bma2x2_get_fifo_framecount err\n");
		return;
	}
	if (count == 0)
		return;
	if (count > MAX_FIFO_F_LEVEL) {
		if (bma2x2_get_mode(bma2x2->bma2x2_client, &mode) < 0) {
			LOG_ERR("bma2x2_get_mode err\n");
			return;
		}
		if (BMA2X2_MODE_NORMAL == mode) {
			LOG_ERR("bma2x2 fifo_count: %d abnormal, op_mode: %d\n",
					count, mode);
			count = MAX_FIFO_F_LEVEL;
		} else {
			/*chip already suspend or shutdown*/
			count = 0;
			return;
		}
	}
	if (bma_i2c_burst_read(bma2x2->bma2x2_client,
			BMA2X2_FIFO_DATA_OUTPUT_REG, fifo_data_out,
						count * f_len) < 0) {
		LOG_ERR("bma2x2 read fifo err\n");
		return;
	}
	bma2x2->fifo_time = bma2x2_get_alarm_timestamp();
	if (bma2x2->acc_count == 0) {
		bma2x2->base_time = bma2x2->timestamp =
			bma2x2->fifo_time - (count-1) * bma2x2->time_odr;
	}
	bma2x2->acc_count += count;
	del = bma2x2->fifo_time - bma2x2->base_time;
	time_internal = div64_u64(del, bma2x2->acc_count);

	data_cnt++;
	if (data_cnt == 1)
		time_odr = bma2x2->time_odr;

	if (time_internal > time_odr) {
		if (time_internal - time_odr > div64_u64 (time_odr, 200))
			time_internal = time_odr + div64_u64(time_odr, 200);
	} else {
		if (time_odr - time_internal > div64_u64(time_odr, 200))
			time_internal = time_odr - div64_u64(time_odr, 200);
	}
/* please give attation for the fifo output data format*/
	if (f_len == 6) {
		/* Select X Y Z axis data output for every frame */
		for (i = 0; i < count; i++) {
			acc_lsb.x =
				((unsigned char)fifo_data_out[i * f_len + 1] << 8
				| (unsigned char)fifo_data_out[i * f_len + 0]);
			acc_lsb.y =
				((unsigned char)fifo_data_out[i * f_len + 3] << 8
				| (unsigned char)fifo_data_out[i * f_len + 2]);
			acc_lsb.z =
				((unsigned char)fifo_data_out[i * f_len + 5] << 8
				| (unsigned char)fifo_data_out[i * f_len + 4]);

#ifndef BMA2X2_SENSOR_IDENTIFICATION_ENABLE
			acc_lsb.x >>=
			(16 - bma2x2_sensor_bitwidth[bma2x2->sensor_type]);
			acc_lsb.y >>=
			(16 - bma2x2_sensor_bitwidth[bma2x2->sensor_type]);
			acc_lsb.z >>=
			(16 - bma2x2_sensor_bitwidth[bma2x2->sensor_type]);
#endif
			ts = ns_to_timespec(bma2x2->timestamp);
			bma2x2_remap_sensor_data(&acc_lsb, bma2x2);
			acc_data.data.x = (int32_t)acc_lsb.x;
			acc_data.data.y = (int32_t)acc_lsb.y;
			acc_data.data.z = (int32_t)acc_lsb.z;
			acc_data.timestamp = (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;

			if (bma2x2_ring_buf_put(acc_data) == 0)
				printk(KERN_INFO "bma2x2 ring buf full head %d, tail %d",
					s_ring_buf_head, s_ring_buf_tail);

			bma2x2->timestamp += time_internal - sample_drift_offset;
			}
	}

	drift_time = bma2x2->timestamp - bma2x2->fifo_time;

	if (data_cnt % 50 == 0) {
		if (ABS(drift_time) > div64_u64(time_odr, 5)) {
			sample_drift_offset =
				div64_s64(drift_time, bma2x2->acc_count - pre_data_cnt);
			pre_data_cnt = bma2x2->acc_count;
			time_odr = time_internal;
		}
	}
}

static enum hrtimer_restart reportdata_timer_fun(
	struct hrtimer *hrtimer)
{
	struct bma2x2_data *client_data =
		container_of(hrtimer, struct bma2x2_data, timer);
	int32_t delay = 0;
	LOG_INFO("entry\n");
	delay = atomic_read(&client_data->delay);
	queue_work(reportdata_wq, &(client_data->report_data_work));
	/*set delay*/
	client_data->work_delay_kt = ns_to_ktime(delay*1000000);
	hrtimer_forward(hrtimer, ktime_get(), client_data->work_delay_kt);

	return HRTIMER_RESTART;
}

static ssize_t bma2x2_enable_timer_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return snprintf(buf, 16, "%d\n", bma2x2->is_timer_running);
}

static ssize_t bma2x2_enable_timer_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (data) {
		if (0 == bma2x2->is_timer_running) {
			mutex_lock(&bma2x2->mutex_ring_buf);
			s_ring_buf_head = s_ring_buf_tail = 0;
			mutex_unlock(&bma2x2->mutex_ring_buf);
			hrtimer_start(&bma2x2->timer,
					ns_to_ktime(
			bma2x2->time_odr),
			HRTIMER_MODE_REL);
			/*enable fifo_mode when init*/
			if (bma2x2_set_fifo_mode(bma2x2->bma2x2_client, 2) < 0)
				LOG_ERR("set fifo_mode falied");
			bma2x2->base_time = 0;
			bma2x2->timestamp = 0;
			bma2x2->is_timer_running = 1;
	}
	} else {
		if (1 == bma2x2->is_timer_running) {
			/*disable fifo_mode when close timer*/
			if (bma2x2_set_fifo_mode(bma2x2->bma2x2_client, 0) < 0)
				LOG_ERR("set fifo_mode falied");
			hrtimer_cancel(&bma2x2->timer);
			bma2x2->is_timer_running = 0;
			bma2x2->base_time = 0;
			bma2x2->timestamp = 0;
			bma2x2->fifo_time = 0;
			bma2x2->acc_count = 0;
			mutex_lock(&bma2x2->mutex_ring_buf);
			s_ring_buf_head = s_ring_buf_tail = 0;
			mutex_unlock(&bma2x2->mutex_ring_buf);
		}
	}
	return count;
}

static ssize_t bma2x2_register_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int address, value;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	sscanf(buf, "%3d %3d", &address, &value);
	if (bma2x2_write_reg(bma2x2->bma2x2_client, (unsigned char)address,
				(unsigned char *)&value) < 0)
		return -EINVAL;
	return count;
}
static ssize_t bma2x2_register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	size_t count = 0;
	u8 reg[0x40];
	int i;

	for (i = 0; i < 0x40; i++) {
		ret = bma2x2_smbus_read_byte(bma2x2->bma2x2_client, i, reg+i);
		if (!ret)
			count += sprintf(&buf[count], "0x%x: 0x%x\n", i,
					 reg[i]);
		else
			count += sprintf(&buf[count], "0x%x: /NA/\n", i);
	}
	return count;
}

static ssize_t bma2x2_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_range(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_range(bma2x2->bma2x2_client, (unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_bandwidth_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_bandwidth(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "0x%02x\n", data);

}

static ssize_t bma2x2_bandwidth_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2->sensor_type == BMA280_TYPE)
		if ((unsigned char) data > 14)
			return -EINVAL;

	if (bma2x2_set_bandwidth(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_mode(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d %d\n", data, bma2x2->bma_mode_enabled);
}

static ssize_t bma2x2_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_mode(bma2x2->bma2x2_client,
		(unsigned char) data, BMA_ENABLED_BSX) < 0)
			return -EINVAL;

	return count;
}

static ssize_t bma2x2_value_cache_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma2x2_data *bma2x2 = input_get_drvdata(input);
	struct bma2x2acc acc_value;

	mutex_lock(&bma2x2->value_mutex);
	acc_value = bma2x2->value;
	mutex_unlock(&bma2x2->value_mutex);

	return sprintf(buf, "%d %d %d\n", acc_value.x, acc_value.y,
			acc_value.z);
}

static ssize_t bma2x2_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma2x2_data *bma2x2 = input_get_drvdata(input);
	struct bma2x2acc acc_value;

	if (!(bma2x2_read_accel_xyz(bma2x2->bma2x2_client, bma2x2->sensor_type,
		&acc_value)))
		return sprintf(buf, "%d %d %d\n", acc_value.x, acc_value.y,
			acc_value.z);
	else {
		LOG_INFO("bma2x2 read accel xyz error\n");
		return -EINVAL;
	}
}

static ssize_t bma2x2_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&bma2x2->delay));

}

static ssize_t bma2x2_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return sprintf(buf, "%u\n", bma2x2->chip_id);

}


static ssize_t bma2x2_place_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	int place = BOSCH_SENSOR_PLACE_UNKNOWN;

	if (NULL != bma2x2->bst_pd)
		place = bma2x2->bst_pd->place;

	return sprintf(buf, "%d\n", place);
}


static ssize_t bma2x2_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	LOG_INFO("buf=%s\n", buf);
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (data > BMA2X2_MAX_DELAY)
		data = BMA2X2_MAX_DELAY;
	delay_ms = data;
	if (data != atomic_read(&bma2x2->delay)) {
#ifdef BMA_ENABLE_NEWDATA_INT
		bma2x2_update_odr(bma2x2->bma2x2_client,
				(int) data);
#endif
		atomic_set(&bma2x2->delay, (unsigned int) data);
	}
	return count;
}


static ssize_t bma2x2_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&bma2x2->enable));

}

static void bma2x2_set_enable(struct device *dev, unsigned char enable)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
    int pre_enable = atomic_read(&bma2x2->enable);

    LOG_INFO("pre_enable=%d,enable=%d\n", pre_enable, enable);
    mutex_lock(&bma2x2->enable_mutex);
    if (enable) {
	if (pre_enable == 0) {
	    bma2x2_set_mode(bma2x2->bma2x2_client,
		    BMA2X2_MODE_NORMAL, BMA_ENABLED_INPUT);
#ifndef BMA_ENABLE_NEWDATA_INT
	    schedule_delayed_work(&bma2x2->work,
		    msecs_to_jiffies(atomic_read(&bma2x2->delay)));
#endif
	    atomic_set(&bma2x2->enable, 1);
	    bma2x2->value.x = 0;
	    bma2x2->value.y = 0;
	    bma2x2->value.z = 0;
	    enable_irq(bma2x2->irq);
	}

    } else {
	if (pre_enable == 1) {
	    bma2x2_set_mode(bma2x2->bma2x2_client,
		    BMA2X2_MODE_SUSPEND, BMA_ENABLED_INPUT);
	    gsensor_flag = 0;
	    disable_irq_nosync(bma2x2->irq);
#ifndef BMA_ENABLE_NEWDATA_INT
	    cancel_delayed_work_sync(&bma2x2->work);
#endif
	    atomic_set(&bma2x2->enable, 0);
	}
    }
    mutex_unlock(&bma2x2->enable_mutex);
}

static ssize_t bma2x2_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int ret;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	LOG_INFO("buf=%s\n", buf);
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if ((data == 0) || (data == 1))
		bma2x2_set_enable(dev, (unsigned char)data);

	if (data == 1) {
	    runirq_cnt = 0;
	    runthread_cnt = 0;
	    sync_cnt = 0;
	    event_gap_cnt = 0;
	    ret = bma2x2_set_offset_x(bma2x2->bma2x2_client, (unsigned char)offset_value[0]);
	    ret = bma2x2_set_offset_y(bma2x2->bma2x2_client, (unsigned char)offset_value[1]);
	    ret = bma2x2_set_offset_z(bma2x2->bma2x2_client, (unsigned char)offset_value[2]);
	}
	if (data == 0)
	    LOG_INFO("runirq_cnt=%d runthread_cnt=%d sync_cnt=%d\n", runirq_cnt, runthread_cnt, sync_cnt);
	return count;
}

static ssize_t bma2x2_chip_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", chip_info);
}

static ssize_t bma2x2_fast_calibration_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

#ifdef CONFIG_SENSORS_BMI058
	if (bma2x2_get_offset_target(bma2x2->bma2x2_client,
				BMI058_OFFSET_TRIGGER_X, &data) < 0)
		return -EINVAL;
#else
	if (bma2x2_get_offset_target(bma2x2->bma2x2_client,
				BMA2X2_OFFSET_TRIGGER_X, &data) < 0)
		return -EINVAL;
#endif

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fast_calibration_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

#ifdef CONFIG_SENSORS_BMI058
	if (bma2x2_set_offset_target(bma2x2->bma2x2_client,
			BMI058_OFFSET_TRIGGER_X, (unsigned char)data) < 0)
		return -EINVAL;
#else
	if (bma2x2_set_offset_target(bma2x2->bma2x2_client,
			BMA2X2_OFFSET_TRIGGER_X, (unsigned char)data) < 0)
		return -EINVAL;
#endif

	if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client, 1) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);

		/*LOG_INFO("wait 2ms cal ready flag is %d\n", tmp); */
		timeout++;
		if (timeout == 50) {
			LOG_INFO("get fast calibration ready error\n");
			return -EINVAL;
		};

	} while (tmp == 0);

	LOG_INFO("x axis fast calibration finished\n");
	return count;
}

static ssize_t bma2x2_fast_calibration_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

#ifdef CONFIG_SENSORS_BMI058
	if (bma2x2_get_offset_target(bma2x2->bma2x2_client,
					BMI058_OFFSET_TRIGGER_Y, &data) < 0)
		return -EINVAL;
#else
	if (bma2x2_get_offset_target(bma2x2->bma2x2_client,
					BMA2X2_OFFSET_TRIGGER_Y, &data) < 0)
		return -EINVAL;
#endif

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fast_calibration_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

#ifdef CONFIG_SENSORS_BMI058
	if (bma2x2_set_offset_target(bma2x2->bma2x2_client,
			BMI058_OFFSET_TRIGGER_Y, (unsigned char)data) < 0)
		return -EINVAL;
#else
	if (bma2x2_set_offset_target(bma2x2->bma2x2_client,
			BMA2X2_OFFSET_TRIGGER_Y, (unsigned char)data) < 0)
		return -EINVAL;
#endif

	if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client, 2) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);

		/*LOG_INFO("wait 2ms cal ready flag is %d\n", tmp);*/
		timeout++;
		if (timeout == 50) {
			LOG_INFO("get fast calibration ready error\n");
			return -EINVAL;
		};

	} while (tmp == 0);

	LOG_INFO("y axis fast calibration finished\n");
	return count;
}

static ssize_t bma2x2_fast_calibration_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{


	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_target(bma2x2->bma2x2_client, 3, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fast_calibration_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_target(bma2x2->bma2x2_client, 3, (unsigned
					char)data) < 0)
		return -EINVAL;

	if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client, 3) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);

		/*LOG_INFO("wait 2ms cal ready flag is %d\n", tmp);*/
		timeout++;
		if (timeout == 50) {
			LOG_INFO("get fast calibration ready error\n");
			return -EINVAL;
		};

	} while (tmp == 0);

	LOG_INFO("z axis fast calibration finished\n");
	return count;
}


static ssize_t bma2x2_SleepDur_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_sleep_duration(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_SleepDur_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_sleep_duration(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_fifo_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_mode(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fifo_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_fifo_mode(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;
	return count;
}



static ssize_t bma2x2_fifo_trig_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_trig(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fifo_trig_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_fifo_trig(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}



static ssize_t bma2x2_fifo_trig_src_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_trig_src(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fifo_trig_src_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_fifo_trig_src(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}


/*!
 * @brief show fifo_data_sel axis definition(Android definition, not sensor HW reg).
 * 0--> x, y, z axis fifo data for every frame
 * 1--> only x axis fifo data for every frame
 * 2--> only y axis fifo data for every frame
 * 3--> only z axis fifo data for every frame
 */
static ssize_t bma2x2_fifo_data_sel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	signed char place = BOSCH_SENSOR_PLACE_UNKNOWN;
	if (bma2x2_get_fifo_data_sel(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

#ifdef CONFIG_SENSORS_BMI058
/*Update BMI058 fifo_data_sel to the BMA2x2 common definition*/
	if (BMI058_FIFO_DAT_SEL_X == data)
		data = BMA2X2_FIFO_DAT_SEL_X;
	else if (BMI058_FIFO_DAT_SEL_Y == data)
		data = BMA2X2_FIFO_DAT_SEL_Y;
#endif

	/*remaping fifo_dat_sel if define virtual place in BSP files*/
	if ((NULL != bma2x2->bst_pd) &&
		(BOSCH_SENSOR_PLACE_UNKNOWN != bma2x2->bst_pd->place)) {
		place = bma2x2->bst_pd->place;
		/* sensor with place 0 needs not to be remapped */
		if ((place > 0) && (place < MAX_AXIS_REMAP_TAB_SZ)) {
			/* BMA2X2_FIFO_DAT_SEL_X: 1, Y:2, Z:3;
			* but bst_axis_remap_tab_dft[i].src_x:0, y:1, z:2
			* so we need to +1*/
			if (BMA2X2_FIFO_DAT_SEL_X == data)
				data = bst_axis_remap_tab_dft[place].src_x + 1;
			else if (BMA2X2_FIFO_DAT_SEL_Y == data)
				data = bst_axis_remap_tab_dft[place].src_y + 1;
		}
	}

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_fifo_framecount_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	unsigned char mode;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_framecount(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	if (data > MAX_FIFO_F_LEVEL) {

		if (bma2x2_get_mode(bma2x2->bma2x2_client, &mode) < 0)
			return -EINVAL;

		if (BMA2X2_MODE_NORMAL == mode) {
			LOG_ERR("bma2x2 fifo_count: %d abnormal, op_mode: %d",
					data, mode);
			data = MAX_FIFO_F_LEVEL;
		} else {
			/*chip already suspend or shutdown*/
			data = 0;
		}
	}

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_fifo_framecount_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	bma2x2->fifo_count = (unsigned int) data;

	return count;
}

static ssize_t bma2x2_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_read_temperature(bma2x2->bma2x2_client, &data) < 0)
		return -EINVAL;

	return sprintf(buf, "%d\n", data);

}

/*!
 * @brief store fifo_data_sel axis definition(Android definition, not sensor HW reg).
 * 0--> x, y, z axis fifo data for every frame
 * 1--> only x axis fifo data for every frame
 * 2--> only y axis fifo data for every frame
 * 3--> only z axis fifo data for every frame
 */
static ssize_t bma2x2_fifo_data_sel_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	signed char place;

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	/*save fifo_data_sel(android definition)*/
	bma2x2->fifo_datasel = (unsigned char) data;

	/*remaping fifo_dat_sel if define virtual place*/
	if ((NULL != bma2x2->bst_pd) &&
		(BOSCH_SENSOR_PLACE_UNKNOWN != bma2x2->bst_pd->place)) {
		place = bma2x2->bst_pd->place;
		/* sensor with place 0 needs not to be remapped */
		if ((place > 0) && (place < MAX_AXIS_REMAP_TAB_SZ)) {
			/*Need X Y axis revesal sensor place: P1, P3, P5, P7 */
			/* BMA2X2_FIFO_DAT_SEL_X: 1, Y:2, Z:3;
			  * but bst_axis_remap_tab_dft[i].src_x:0, y:1, z:2
			  * so we need to +1*/
			if (BMA2X2_FIFO_DAT_SEL_X == data)
				data =  bst_axis_remap_tab_dft[place].src_x + 1;
			else if (BMA2X2_FIFO_DAT_SEL_Y == data)
				data =  bst_axis_remap_tab_dft[place].src_y + 1;
		}
	}
#ifdef CONFIG_SENSORS_BMI058
	/*Update BMI058 fifo_data_sel to the BMA2x2 common definition*/
		if (BMA2X2_FIFO_DAT_SEL_X == data)
			data = BMI058_FIFO_DAT_SEL_X;
		else if (BMA2X2_FIFO_DAT_SEL_Y == data)
			data = BMI058_FIFO_DAT_SEL_Y;

#endif
	if (bma2x2_set_fifo_data_sel(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static int bma2x2_ring_buf_empty(void)
{
	return s_ring_buf_head == s_ring_buf_tail;
}

static int bma2x2_ring_buf_full(void)
{
	return (s_ring_buf_tail + 1) % BMA2X2_RING_BUF_SIZE == s_ring_buf_head;
}

static int bma2x2_ring_buf_put(bma2x2_buf_t data)
{
	if (bma2x2_ring_buf_full())
		return 0;

	bma2x2_ring_buf[s_ring_buf_tail].data.x = data.data.x;
	bma2x2_ring_buf[s_ring_buf_tail].data.y = data.data.y;
	bma2x2_ring_buf[s_ring_buf_tail].data.z = data.data.z;
	bma2x2_ring_buf[s_ring_buf_tail].timestamp = data.timestamp;
/*	printk(KERN_ERR "put data %d, %d, %d, %lld", bma2x2_ring_buf[s_ring_buf_tail].data.x,
		bma2x2_ring_buf[s_ring_buf_tail].data.y,
		bma2x2_ring_buf[s_ring_buf_tail].data.z,
		bma2x2_ring_buf[s_ring_buf_tail].timestamp);
*/
	s_ring_buf_tail = (s_ring_buf_tail + 1) % BMA2X2_RING_BUF_SIZE;
	return 1;
}

static int bma2x2_ring_buf_get(bma2x2_buf_t *pData)
{
	if (bma2x2_ring_buf_empty())
		return 0;

	pData->data.x = bma2x2_ring_buf[s_ring_buf_head].data.x;
	pData->data.y = bma2x2_ring_buf[s_ring_buf_head].data.y;
	pData->data.z = bma2x2_ring_buf[s_ring_buf_head].data.z;
	pData->timestamp = bma2x2_ring_buf[s_ring_buf_head].timestamp;

	s_ring_buf_head = (s_ring_buf_head + 1) % BMA2X2_RING_BUF_SIZE;

	return 1;
}

static ssize_t bma2x2_fifo_data_out_frame_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	int len;
	bma2x2_buf_t data;
	char  *tmp = buf;

	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (buf == NULL)
		return 0;
	mutex_lock(&bma2x2->mutex_ring_buf);

	while (bma2x2_ring_buf_get(&data) != 0) {
		len = sprintf(tmp, "%d %d %d %llu ",
			data.data.x, data.data.y, data.data.z, data.timestamp);
		tmp += len;
		err += len;
		if (err >= (BMA2X2_RING_BUF_SIZE-1) * sizeof (bma2x2_buf_t))
			break;
	}
	mutex_unlock(&bma2x2->mutex_ring_buf);

	return err;
}

static ssize_t bma2x2_offset_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_x(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_offset_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_x(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_offset_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_y(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_offset_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_y(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_offset_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_z(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_offset_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_z(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_driver_version_show(struct device *dev
		, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	int ret;

	if (bma2x2 == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}

	ret = snprintf(buf, 128, "Driver version: %s\n",
			DRIVER_VERSION);
	return ret;
}

static DEVICE_ATTR(range, 0644,
		bma2x2_range_show, bma2x2_range_store);
static DEVICE_ATTR(bandwidth, 0644,
		bma2x2_bandwidth_show, bma2x2_bandwidth_store);
static DEVICE_ATTR(op_mode, 0644,
	bma2x2_mode_show, bma2x2_mode_store);
static DEVICE_ATTR(value, S_IRUGO,
		bma2x2_value_show, NULL);
static DEVICE_ATTR(value_cache, S_IRUGO,
		bma2x2_value_cache_show, NULL);
static DEVICE_ATTR(acc_delay, 0644,
		bma2x2_delay_show, bma2x2_delay_store);
static DEVICE_ATTR(acc_enable, 0644,
		bma2x2_enable_show, bma2x2_enable_store);
static DEVICE_ATTR(acc_chip_info, 0444,
		bma2x2_chip_info_show, NULL);
static DEVICE_ATTR(SleepDur, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_SleepDur_show, bma2x2_SleepDur_store);
static DEVICE_ATTR(fast_calibration_x, 0644,
		bma2x2_fast_calibration_x_show,
		bma2x2_fast_calibration_x_store);
static DEVICE_ATTR(fast_calibration_y, 0644,
		bma2x2_fast_calibration_y_show,
		bma2x2_fast_calibration_y_store);
static DEVICE_ATTR(fast_calibration_z, 0644,
		bma2x2_fast_calibration_z_show,
		bma2x2_fast_calibration_z_store);
static DEVICE_ATTR(fifo_mode, 0644,
		bma2x2_fifo_mode_show, bma2x2_fifo_mode_store);
static DEVICE_ATTR(fifo_framecount, 0644,
		bma2x2_fifo_framecount_show, bma2x2_fifo_framecount_store);
static DEVICE_ATTR(fifo_trig, 0644,
		bma2x2_fifo_trig_show, bma2x2_fifo_trig_store);
static DEVICE_ATTR(fifo_trig_src, 0644,
		bma2x2_fifo_trig_src_show, bma2x2_fifo_trig_src_store);
static DEVICE_ATTR(fifo_data_sel, 0644,
		bma2x2_fifo_data_sel_show, bma2x2_fifo_data_sel_store);
static DEVICE_ATTR(fifo_data_frame, S_IRUGO,
		bma2x2_fifo_data_out_frame_show, NULL);
static DEVICE_ATTR(reg, 0644,
		bma2x2_register_show, bma2x2_register_store);
static DEVICE_ATTR(chip_id, S_IRUGO,
		bma2x2_chip_id_show, NULL);
static DEVICE_ATTR(offset_x, 0644,
		bma2x2_offset_x_show,
		bma2x2_offset_x_store);
static DEVICE_ATTR(offset_y, 0644,
		bma2x2_offset_y_show,
		bma2x2_offset_y_store);
static DEVICE_ATTR(offset_z, 0644,
		bma2x2_offset_z_show,
		bma2x2_offset_z_store);
static DEVICE_ATTR(enable_int, 0200,
		NULL, bma2x2_enable_int_store);
static DEVICE_ATTR(int_mode, 0644,
		bma2x2_int_mode_show, bma2x2_int_mode_store);
static DEVICE_ATTR(slope_duration, 0644,
		bma2x2_slope_duration_show, bma2x2_slope_duration_store);
static DEVICE_ATTR(slope_threshold, 0644,
		bma2x2_slope_threshold_show, bma2x2_slope_threshold_store);
static DEVICE_ATTR(slope_no_mot_duration, 0644,
		bma2x2_slope_no_mot_duration_show,
			bma2x2_slope_no_mot_duration_store);
static DEVICE_ATTR(slope_no_mot_threshold, 0644,
		bma2x2_slope_no_mot_threshold_show,
			bma2x2_slope_no_mot_threshold_store);
static DEVICE_ATTR(high_g_duration, 0644,
		bma2x2_high_g_duration_show, bma2x2_high_g_duration_store);
static DEVICE_ATTR(high_g_threshold, 0644,
		bma2x2_high_g_threshold_show, bma2x2_high_g_threshold_store);
static DEVICE_ATTR(low_g_duration, 0644,
		bma2x2_low_g_duration_show, bma2x2_low_g_duration_store);
static DEVICE_ATTR(low_g_threshold, 0644,
		bma2x2_low_g_threshold_show, bma2x2_low_g_threshold_store);
static DEVICE_ATTR(tap_duration, 0644,
		bma2x2_tap_duration_show, bma2x2_tap_duration_store);
static DEVICE_ATTR(tap_threshold, 0644,
		bma2x2_tap_threshold_show, bma2x2_tap_threshold_store);
static DEVICE_ATTR(tap_quiet, 0644,
		bma2x2_tap_quiet_show, bma2x2_tap_quiet_store);
static DEVICE_ATTR(tap_shock, 0644,
		bma2x2_tap_shock_show, bma2x2_tap_shock_store);
static DEVICE_ATTR(tap_samp, 0644,
		bma2x2_tap_samp_show, bma2x2_tap_samp_store);
static DEVICE_ATTR(orient_mode, 0644,
		bma2x2_orient_mode_show, bma2x2_orient_mode_store);
static DEVICE_ATTR(orient_blocking, 0644,
		bma2x2_orient_blocking_show, bma2x2_orient_blocking_store);
static DEVICE_ATTR(orient_hyst, 0644,
		bma2x2_orient_hyst_show, bma2x2_orient_hyst_store);
static DEVICE_ATTR(orient_theta, 0644,
		bma2x2_orient_theta_show, bma2x2_orient_theta_store);
static DEVICE_ATTR(flat_theta, 0644,
		bma2x2_flat_theta_show, bma2x2_flat_theta_store);
static DEVICE_ATTR(flat_hold_time, 0644,
		bma2x2_flat_hold_time_show, bma2x2_flat_hold_time_store);
static DEVICE_ATTR(selftest, 0644,
		bma2x2_selftest_show, bma2x2_selftest_store);
static DEVICE_ATTR(softreset, 0200,
		NULL, bma2x2_softreset_store);
static DEVICE_ATTR(enable_timer, 0644,
		bma2x2_enable_timer_show, bma2x2_enable_timer_store);
static DEVICE_ATTR(temperature, S_IRUGO,
		bma2x2_temperature_show, NULL);
static DEVICE_ATTR(place, S_IRUGO,
		bma2x2_place_show, NULL);
static DEVICE_ATTR(driver_version, S_IRUGO,
		bma2x2_driver_version_show, NULL);

static struct attribute *bma2x2_attributes[] = {
	&dev_attr_range.attr,
	&dev_attr_bandwidth.attr,
	&dev_attr_op_mode.attr,
	&dev_attr_value.attr,
	&dev_attr_value_cache.attr,
	&dev_attr_acc_delay.attr,
	&dev_attr_acc_enable.attr,
	&dev_attr_acc_chip_info.attr,
	&dev_attr_SleepDur.attr,
	&dev_attr_reg.attr,
	&dev_attr_fast_calibration_x.attr,
	&dev_attr_fast_calibration_y.attr,
	&dev_attr_fast_calibration_z.attr,
	&dev_attr_fifo_mode.attr,
	&dev_attr_fifo_framecount.attr,
	&dev_attr_fifo_trig.attr,
	&dev_attr_fifo_trig_src.attr,
	&dev_attr_fifo_data_sel.attr,
	&dev_attr_fifo_data_frame.attr,
	&dev_attr_chip_id.attr,
	&dev_attr_offset_x.attr,
	&dev_attr_offset_y.attr,
	&dev_attr_offset_z.attr,
	&dev_attr_enable_int.attr,
	&dev_attr_enable_timer.attr,
	&dev_attr_int_mode.attr,
	&dev_attr_slope_duration.attr,
	&dev_attr_slope_threshold.attr,
	&dev_attr_slope_no_mot_duration.attr,
	&dev_attr_slope_no_mot_threshold.attr,
	&dev_attr_high_g_duration.attr,
	&dev_attr_high_g_threshold.attr,
	&dev_attr_low_g_duration.attr,
	&dev_attr_low_g_threshold.attr,
	&dev_attr_tap_threshold.attr,
	&dev_attr_tap_duration.attr,
	&dev_attr_tap_quiet.attr,
	&dev_attr_tap_shock.attr,
	&dev_attr_tap_samp.attr,
	&dev_attr_orient_mode.attr,
	&dev_attr_orient_blocking.attr,
	&dev_attr_orient_hyst.attr,
	&dev_attr_orient_theta.attr,
	&dev_attr_flat_theta.attr,
	&dev_attr_flat_hold_time.attr,
	&dev_attr_selftest.attr,
	&dev_attr_softreset.attr,
	&dev_attr_temperature.attr,
	&dev_attr_place.attr,
	&dev_attr_driver_version.attr,
	NULL
};

static struct attribute_group bma2x2_attribute_group = {
	.attrs = bma2x2_attributes
};

#if defined(BMA2X2_ENABLE_INT1) || defined(BMA2X2_ENABLE_INT2)
unsigned char *orient[] = {"upward looking portrait upright",
	"upward looking portrait upside-down",
		"upward looking landscape left",
		"upward looking landscape right",
		"downward looking portrait upright",
		"downward looking portrait upside-down",
		"downward looking landscape left",
		"downward looking landscape right"};
#if 0
static void bma2x2_irq_work_func(struct work_struct *work)
{
	struct bma2x2_data *bma2x2 = container_of((struct work_struct *)work,
			struct bma2x2_data, irq_work);

#ifdef BMA_ENABLE_NEWDATA_INT
	/* bma253 hold new data INT about 270us~390us */
	/* check new_data_x if data updated */
	struct bma2x2acc acc;

	bma2x2_read_accel_xyz(bma2x2->bma2x2_client,
				bma2x2->sensor_type, &acc);
	/* LOG_INFO("New data x=%d,y=%d,z=%d\n", acc.x, acc.y, acc.z); */
	input_report_abs(bma2x2->input, ABS_X, acc.x);
	input_report_abs(bma2x2->input, ABS_Y, acc.y);
	input_report_abs(bma2x2->input, ABS_Z, acc.z);
	input_sync(bma2x2->input);
	mutex_lock(&bma2x2->value_mutex);
	bma2x2->value = acc;
	mutex_unlock(&bma2x2->value_mutex);
	enable_irq(bma2x2->irq);
	return;
#endif
}
#endif

static irqreturn_t bma2x2_irq_handler(int irq, void *handle)
{
	struct bma2x2_data *data = handle;

	if (data == NULL)
		return IRQ_HANDLED;
	if (data->bma2x2_client == NULL)
		return IRQ_HANDLED;

	disable_irq_nosync(data->irq);
	/* schedule_work(&data->irq_work); */
	acc_time_monoboot =  ktime_get_boottime();
	gsensor_flag = 1;
	wake_up_interruptible_sync(&waiter);
	runirq_cnt++;
	return IRQ_HANDLED;
}
#endif /* defined(BMA2X2_ENABLE_INT1)||defined(BMA2X2_ENABLE_INT2) */

#ifdef CONFIG_OF
static void bma_parse_dt(struct device *dev, struct bma2x2_data *data)
{
	struct device_node *np = dev->of_node;
	unsigned int tmp;
	int ret;

	data->gpio_num = of_get_gpio(np, 0);
	if (data->gpio_num < 0) {
		dev_err(dev, "fail to get irq_gpio_number\n");
		LOG_INFO(" get irq1_gpio_number fail\n");
	}

	ret = of_property_read_u32(np, "offset-value-x", &tmp);
	if (ret) {
		dev_err(dev, "property offset-value-x not found\n");
		offset_value[0] = 0;
	} else {
		offset_value[0] = tmp;
	}
	ret = of_property_read_u32(np, "offset-value-y", &tmp);
	if (ret) {
		dev_err(dev, "property offset-value-y not found\n");
		offset_value[1] = 0;
	} else {
		offset_value[1] = tmp;
	}
	ret = of_property_read_u32(np, "offset-value-z", &tmp);
	if (ret) {
		dev_err(dev, "property offset-value-z not found\n");
		offset_value[2] = 0;
	} else {
		offset_value[2] = tmp;
	}
}
#endif


static int bma2x2_event_handler(void *data)
{
	int error = 0;
	struct sched_param param = {.sched_priority = 99};

	sched_setscheduler(current, SCHED_RR, &param);
	do {
		error = wait_event_interruptible(waiter, (gsensor_flag != 0));
		if (error) {
			LOG_ERR("error= %d\n", error);
			return error;
		}
		gsensor_flag = 0;
		bma2x2_update_data(data);

	} while (!kthread_should_stop());

	return 0;
}

static noinline void bma2x2_update_data(void *u)
{
	int rd_result = -1;
	struct bma2x2_data *bma2x2 = u;
#ifdef BMA_ENABLE_NEWDATA_INT
	/* bma253 hold new data INT about 270us~390us */
	/* check new_data_x if data updated */
	struct bma2x2acc acc;

	ktime_t time_mono_before;
	ktime_t time_mono_after;
	s64 time_us_before;
	s64 time_us_after;
	s64 time_us_delta;

	runthread_cnt++;
	time_mono_before = ktime_get();

	if (filter_flag) {
		filter_flag = 0;
		enable_irq(bma2x2->irq);
		return;
	}
	/*
	  * when odr updated, the sensor need 3 data frame times
	  * or 200ms to get stable
	  */
	if ((odr_delay == 0) || ((bma2x2->value.x == 0) &&
		(bma2x2->value.y == 0) && (bma2x2->value.z == 0))) {
		atomic_inc(&bma2x2->i2c_busy);
		/* rd_sesult < 0, i2c read error */
		rd_result = bma2x2_read_accel_xyz(bma2x2->bma2x2_client,
					bma2x2->sensor_type, &acc);
		atomic_dec(&bma2x2->i2c_busy);

		mutex_lock(&bma2x2->value_mutex);
		bma2x2->value = acc;
		mutex_unlock(&bma2x2->value_mutex);
	} else {
		odr_delay--;
		acc.x = bma2x2->value.x;
		acc.y = bma2x2->value.y;
		acc.z = bma2x2->value.z;
	}
	time_mono_after = ktime_get();
	time_us_before = ktime_to_us(time_mono_before);
	time_us_after = ktime_to_us(time_mono_after);
	time_us_delta = time_us_after - time_us_before;
	/* i2c read latency should in 3ms */
	if (time_us_delta > 9000)
		LOG_INFO("exception: runtime_delta_us =%lld\n", time_us_delta);

	/* LOG_INFO("New data x=%d,y=%d,z=%d\n", acc.x, acc.y, acc.z); */
	if (rd_result < 0)
	    LOG_INFO("exception: iic read error rd_result=%d\n", rd_result);
	/* if rd_result<0 iic read error,report last data */
	/*LOG_INFO("delay_ms=%ld,c1000=%d c200=%d\n", delay_ms, c1000, c200);*/
	if (delay_ms == 200) {
		if (c200 == 2) {
			input_report_abs(bma2x2->input, ABS_X, acc.x);
			input_report_abs(bma2x2->input, ABS_Y, acc.y);
			input_report_abs(bma2x2->input, ABS_Z, acc.z);
			input_sync(bma2x2->input);
			c200 = 0;
		} else
			c200++;
	} else {
		input_report_abs(bma2x2->input, ABS_X, acc.x);
		input_report_abs(bma2x2->input, ABS_Y, acc.y);
		input_report_abs(bma2x2->input, ABS_Z, acc.z);
		input_sync(bma2x2->input);
	}

	enable_irq(bma2x2->irq);
	return;
#endif
}

static int bma2x2_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
	int free_flag = 0;
	struct bma2x2_data *data;
	struct input_dev *dev;
	struct bst_dev  *dev_acc;
#if defined(BMA2X2_ENABLE_INT1) || defined(BMA2X2_ENABLE_INT2)
	struct bosch_sensor_specific *pdata;
#endif
	struct device_node *np;

	LOG_INFO("bma2x2_probe start\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		LOG_ERR("i2c_check_functionality error\n");
		err = -EIO;
		goto exit;
	}
	data = kzalloc(sizeof(struct bma2x2_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	mutex_init(&data->value_mutex);
	mutex_init(&data->mode_mutex);
	mutex_init(&data->enable_mutex);
	mutex_init(&data->mutex_ring_buf);

	/* do soft reset */
	mdelay(5);
	i2c_set_clientdata(client, data);
	data->bma2x2_client = client;
	if (bma2x2_soft_reset(client) < 0) {
		LOG_ERR("i2c bus write error, pls check HW connection\n");
		err = -EINVAL;
		goto kfree_exit;
	}
	mdelay(5);
	/* read and check chip id */
	if (bma2x2_check_chip_id(client, data) < 0) {
		err = -EINVAL;
		goto kfree_exit;
	}

	bma2x2_set_bandwidth(client, BMA2X2_BW_SET_DEFAULT);
	bma2x2_set_range(client, BMA2X2_RANGE_SET_DEFAULT);

#if defined(BMA2X2_ENABLE_INT1) || defined(BMA2X2_ENABLE_INT2)
	pdata = client->dev.platform_data;
	if (pdata) {
		if (pdata->irq_gpio_cfg && (pdata->irq_gpio_cfg() < 0)) {
			LOG_ERR("IRQ GPIO conf. error %d\n",
				client->irq);
		}
	}

#ifdef CONFIG_OF
	np = client->dev.of_node;
	if (np && data)
		bma_parse_dt(&client->dev, data);
#endif

#ifdef BMA2X2_ENABLE_INT1
	#ifdef BMA_ENABLE_NEWDATA_INT
	bma2x2_set_newdata(client, BMA2X2_INT1_NDATA, 1);
	bma2x2_set_newdata(client, BMA2X2_INT2_NDATA, 0);
	#endif
#endif

#ifdef BMA2X2_ENABLE_INT2
	/* maps interrupt to INT2 pin */
	bma2x2_set_int2_pad_sel(client, PAD_LOWG);
	bma2x2_set_int2_pad_sel(client, PAD_HIGHG);
	bma2x2_set_int2_pad_sel(client, PAD_SLOP);
	bma2x2_set_int2_pad_sel(client, PAD_DOUBLE_TAP);
	bma2x2_set_int2_pad_sel(client, PAD_SINGLE_TAP);
	bma2x2_set_int2_pad_sel(client, PAD_ORIENT);
	bma2x2_set_int2_pad_sel(client, PAD_FLAT);
	bma2x2_set_int2_pad_sel(client, PAD_SLOW_NO_MOTION);
#ifdef BMA_ENABLE_NEWDATA_INT
	bma2x2_set_newdata(client, BMA2X2_INT1_NDATA, 0);
	bma2x2_set_newdata(client, BMA2X2_INT2_NDATA, 1);
#endif
#endif
	/*latch interrupt 250ms,exception for new data int */
	bma2x2_set_Int_Mode(client, 1);

	/* do not open any interrupt here  */
	/* bma2x2_set_Int_Enable(client, 10, 1);*/
	/* bma2x2_set_Int_Enable(client, 11, 1); */
       err = gpio_request(data->gpio_num, "acceleremter_INT1");
	if (err) {
	    LOG_ERR("gpio pin request fail (%d)\n", err);
	    err = -EINVAL;
	    goto kfree_exit;
	}  else  {
	    gpio_direction_input(data->gpio_num);
	    LOG_INFO("gpio%d  status is [%d]\n", data->gpio_num, gpio_get_value(data->gpio_num));
	    /*get irq*/
	    data->irq  = gpio_to_irq(data->gpio_num);
	    LOG_INFO("irq1 number is %d\n", data->irq);
	}
	if (data->irq > 0) {
	    err = request_irq(data->irq, bma2x2_irq_handler, IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND,
		    "bma2x2", data);
	    disable_irq_nosync(data->irq);
	}
	if (err) {
	    goto err_request_irq;
	    LOG_ERR("could not request irq %d\n", data->irq);
	}
#endif

#ifndef BMA_ENABLE_NEWDATA_INT
	INIT_DELAYED_WORK(&data->work, bma2x2_work_func);
#endif
	atomic_set(&data->delay, BMA2X2_MAX_DELAY);
	atomic_set(&data->enable, 0);
	data->on_before_suspend = 0;

	dev = input_allocate_device();
	if (!dev) {
		err =  -ENOMEM;
		goto err_input_allocation;
	}
	/* only value events reported */
	dev->name = "accelerometer";
	dev->id.bustype = BUS_I2C;
	input_set_capability(dev, EV_ABS, ABS_MISC);
	/* the flat -1 indicates input core always report to userspace */
	/* not ignore the data if it is same as last one */
	input_set_abs_params(dev, ABS_X, -G_MAX, G_MAX, 0, -1);
	input_set_abs_params(dev, ABS_Y, -G_MAX, G_MAX, 0, -1);
	input_set_abs_params(dev, ABS_Z, -G_MAX, G_MAX, 0, -1);
	input_set_drvdata(dev, data);
	err = input_register_device(dev);
	if (err < 0)
		goto err_register_input_device;

	data->input = dev;

	err = sysfs_create_group(&data->input->dev.kobj,
			&bma2x2_attribute_group);
	if (err < 0)
		goto error_sysfs;
	kobject_uevent(&data->input->dev.kobj, KOBJ_ADD);

	dev_acc = bst_allocate_device();
	if (!dev_acc) {
		err = -ENOMEM;
		goto error_sysfs;
	}
	dev_acc->name = ACC_NAME;

	bst_set_drvdata(dev_acc, data);

	err = bst_register_device(dev_acc);
	if (err < 0)
		goto bst_free_acc_exit;

	data->bst_acc = dev_acc;
	err = sysfs_create_group(&data->bst_acc->dev.kobj,
			&bma2x2_attribute_group);
	if (err < 0)
		goto bst_free_exit;

	if (NULL != client->dev.platform_data) {
		data->bst_pd = kzalloc(sizeof(*data->bst_pd),
				GFP_KERNEL);

		if (NULL != data->bst_pd) {
			memcpy(data->bst_pd, client->dev.platform_data,
					sizeof(*data->bst_pd));
			LOG_INFO("%s sensor driver set place: p%d",
				data->bst_pd->name, data->bst_pd->place);
		}
	}

	INIT_WORK(&data->report_data_work,
	bma2x2_timer_work_fun);
	reportdata_wq = create_singlethread_workqueue("bma2x2_wq");
	if (NULL == reportdata_wq)
		LOG_ERR("fail to create the reportdta_wq");
	hrtimer_init(&data->timer, CLOCK_MONOTONIC,
		HRTIMER_MODE_REL);
	data->timer.function = reportdata_timer_fun;
	data->work_delay_kt = ns_to_ktime(10000000);
	data->is_timer_running = 0;
	data->timestamp = 0;
	data->time_odr = 4000000;/*default bandwidth 125HZ*/
	data->bma_mode_enabled = 0;
	data->fifo_datasel = 0;
	data->fifo_count = 0;
	data->acc_count = 0;

#ifdef BMA_ENABLE_NEWDATA_INT
	atomic_set(&data->i2c_busy, 0);
	/* enable new data interrupt */
	bma2x2_set_Int_Enable(client, 4, 1);
	/* INIT_WORK(&data->irq_work, bma2x2_irq_work_func); */
	thread = kthread_run(bma2x2_event_handler, data, "gsensor-wait");
	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		LOG_ERR("failed to create kernel thread: %d\n", err);
		goto bst_free_exit;
	}
#endif
	/* bma253 default mode is normal mode */
	if (bma2x2_set_mode(client, BMA2X2_MODE_SUSPEND, BMA_ENABLED_ALL) < 0) {
#ifdef BMA_ENABLE_NEWDATA_INT
		kthread_stop(thread);
#endif
		err = -EINVAL;
		goto bst_free_exit;
	}
	LOG_INFO("BMA2x2 driver probe successfully\n");
	return 0;

bst_free_exit:
	bst_unregister_device(dev_acc);
bst_free_acc_exit:
	bst_free_device(dev_acc);
error_sysfs:
	input_free_device(dev);
	input_unregister_device(data->input);
	free_flag = 1;
err_register_input_device:
	if (free_flag == 0)
		input_free_device(dev);
err_input_allocation:
err_request_irq:
	if (data->irq) {
		free_irq(data->irq, data);
		gpio_free(data->gpio_num);
	}
kfree_exit:
	LOG_ERR("probe error entry kfree_exit\n");
	/* bma253 default mode is normal mode */
	if (bma2x2_set_mode(client, BMA2X2_MODE_SUSPEND, BMA_ENABLED_ALL) < 0)
		LOG_ERR("bma253 is normal mode\n");
	if ((NULL != data) && (NULL != data->bst_pd)) {
		kfree(data->bst_pd);
		data->bst_pd = NULL;
	}
	kfree(data);
exit:
	return err;
}

static int bma2x2_remove(struct i2c_client *client)
{
	struct bma2x2_data *data = i2c_get_clientdata(client);

	if (NULL == data)
		return 0;

	bma2x2_set_enable(&client->dev, 0);
#ifdef BMA_ENABLE_NEWDATA_INT
	kthread_stop(thread);
#endif
	sysfs_remove_group(&data->input->dev.kobj, &bma2x2_attribute_group);
	input_unregister_device(data->input);

	if (NULL != data->bst_pd) {
		kfree(data->bst_pd);
		data->bst_pd = NULL;
	}

	kfree(data);
	return 0;
}

void bma2x2_shutdown(struct i2c_client *client)
{
	struct bma2x2_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	bma2x2_set_mode(data->bma2x2_client,
		BMA2X2_MODE_DEEP_SUSPEND, BMA_ENABLED_ALL);
	mutex_unlock(&data->enable_mutex);
}

static int bma2x2_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

    if (bma2x2 != NULL) {
	bma2x2->on_before_suspend = atomic_read(&bma2x2->enable);
	LOG_INFO(" -- on_before_suspend= %d-- !\n", bma2x2->on_before_suspend);
	if (bma2x2->on_before_suspend)
	    bma2x2_set_enable(&client->dev, 0);
	while (atomic_read(&bma2x2->i2c_busy))
	schedule();
    }
	LOG_INFO("--exit--\n");
    return 0;
}

static int bma2x2_acc_resume(struct i2c_client *client)
{
    struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

    if (bma2x2 != NULL) {
	LOG_INFO(" -- on_before_suspend= %d-- !\n", bma2x2->on_before_suspend);
	if (bma2x2->on_before_suspend)
	    bma2x2_set_enable(&client->dev, 1);
    }
    return 0;
}

static int bma2x2_pm_suspend(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    pm_message_t mesg = {0};

    bma2x2_acc_suspend(client, mesg);
    return 0;
}

static int bma2x2_pm_resume(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);

    bma2x2_acc_resume(client);
    return 0;
}

static const struct dev_pm_ops bma2x2_pm_ops = {
    .suspend = bma2x2_pm_suspend,
    .resume = bma2x2_pm_resume,
};

static const struct i2c_device_id bma2x2_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, bma2x2_id);
static const struct of_device_id bma2x2_of_match[] = {
	{ .compatible = "BOSCH,bma2x2", },
	{ }
};
MODULE_DEVICE_TABLE(of, bma2x2_of_match);

static struct i2c_driver bma2x2_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = SENSOR_NAME,
		.of_match_table = bma2x2_of_match,
		.pm = &bma2x2_pm_ops
	},
	.id_table   = bma2x2_id,
	.probe      = bma2x2_probe,
	.remove     = bma2x2_remove,
	.shutdown   = bma2x2_shutdown,
};

static int __init BMA2X2_init(void)
{
	return i2c_add_driver(&bma2x2_driver);
}

static void __exit BMA2X2_exit(void)
{
	i2c_del_driver(&bma2x2_driver);
}

MODULE_AUTHOR("bao.yue@spreadtrum.com");
MODULE_DESCRIPTION("BMA2X2 ACCELEROMETER SENSOR DRIVER");
MODULE_LICENSE("GPL v2");

module_init(BMA2X2_init);
module_exit(BMA2X2_exit);
