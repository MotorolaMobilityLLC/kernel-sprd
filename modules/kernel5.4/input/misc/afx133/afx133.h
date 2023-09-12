/******************** (C) COPYRIGHT Voltafield 2014 ********************
*
* File Name          : af6133.h
* Authors            : Production, CAE Team
*                    : Gary Huang
* Date               : 2018/MarJan/22
* Description        : F6133 Magnetic sensor Driver header file
*
************************************************************************
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
*******************************************************************************/
/*******************************************************************************
Version History.

20150326  1st version
20150706  new arch. for qcom
*******************************************************************************/

#ifndef __AF6133_H__
#define __AF6133_H__

#define AF6133_I2C_DEV_NAME            "af6133"
#define AF6133_INPUT_DEV_NAME          "compass"
#define AF6133_SYS_CLS_NAME            "compass"
#define AF6133_SYS_DEV_NAME            "msensor"

#define AF6133J_PID	0x64

/* register definition */
#define REG_PCODE       0x00
#define REG_DATA	      0x03
#define REG_MEASURE	    0x0A
#define REG_RANGE	      0x0B
#define REG_I2C_CHECK   0x10
#define REG_SW_RESET	  0x11
#define REG_AVG		      0x13
#define REG_XY_SR	      0x14
#define REG_OTP_1D      0x1D
#define REG_TEMP        0x21
#define REG_DATA2	      0x23
#define REG_OSR		      0x2D
#define REG_SECURITY	  0x30
#define REG_WAITING	    0x32
#define REG_Z_SR	      0x33
#define REG_MUX_SEL     0x34
#define REG_TEST0	      0x35
#define REG_TEST2	      0x37


/* offset parameters */
#define MAG_MIN_OFFSET	447
#define MAG_OFFSET_LOOP	5

/* BIST parameters */
#define MAG_BIST_LOOP	5

#define BIST_COEFF_X	75 // 0.7476
#define BIST_COEFF_Y	84 // 0.8394
#define BIST_COEFF_Z	49 // 0.4853

#define BIST_BIAS_0	0
#define BIST_BIAS_1	0
#define BIST_BIAS_2	0
#define BIST_BIAS_3	0

#define BIST_GAIN_COEFF_X	(int32_t)(10000 * 500 / BIST_COEFF_X)
#define BIST_GAIN_COEFF_Y	(int32_t)(10000 * 500 / BIST_COEFF_Y)
#define BIST_GAIN_COEFF_Z	(int32_t)(10000 * 500 / BIST_COEFF_Z)

#define BIST_COMP_COEFF_0	(int16_t)(100 * BIST_COEFF_Y / BIST_COEFF_X)
#define BIST_COMP_COEFF_1	(int16_t)(100 * BIST_COEFF_X / BIST_COEFF_Y)
#define BIST_COMP_COEFF_2	(int16_t)(100 * BIST_COEFF_X / BIST_COEFF_Z)
#define BIST_COMP_COEFF_3	(int16_t)(100 * BIST_COEFF_Y / BIST_COEFF_Z)

#define BIST_COMP_BIAS_0	-1 //(int16_t)(sin(BIST_BIAS_0 * VTC_DEG2RAD)*100)
#define BIST_COMP_BIAS_1	-1 //(int16_t)(sin(BIST_BIAS_1 * VTC_DEG2RAD)*100)
#define BIST_COMP_BIAS_2	11 //(int16_t)(sin(BIST_BIAS_2 * VTC_DEG2RAD)*100)
#define BIST_COMP_BIAS_3	 1 //(int16_t)(sin(BIST_BIAS_3 * VTC_DEG2RAD)*100)

#endif  /* __AF6133_H__ */

