/******************** (C) COPYRIGHT Voltafield 2014 ********************
*
* File Name          : af6133.c
* Authors            : Production, CAE Team
*                    : Gary Huang
* Date               : 2018/Jan/22
* Description        : AF6133 Magneto sensor Driver
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

20180122  1st version
*******************************************************************************/

#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>

//#include <linux/af6133.h>
#include "afx133.h"
//#include <linux/sensors.h>

#define AF6133_DEBUG 0
#define AF6133_DRIVER_VERSION     "1.0.1"

#define DELAY_TIME_DEFAULT         50   //ms, 20Hz
#define DELAY_TIME_MIN             10   //ms, 100Hz
#define DELAY_TIME_MAX            100   //ms, 10Hz

static struct i2c_client *this_client;

struct af6133_platform_data {
	unsigned int poll_interval;
	int offset[3];
	int gain[3];
	int comp[4];
	int bist_x[3];
	int bist_y[3];
	int bist_z[3];
};

struct af6133_data {
        struct i2c_client *client;
        struct device *class_dev;
        struct class *compass;
        struct af6133_platform_data *pdata;
        struct mutex lock;
        struct input_dev *input;
        struct delayed_work work;
        atomic_t enabled;
};

static int VTC_i2c_Rx(char *rxData, int length)
{
        uint8_t retry;
        struct i2c_msg msgs[] =
        {
                {
                        .addr = this_client->addr,
                        .flags = 0,
                        .len = 1,
                        .buf = rxData,
                },
                {
                        .addr = this_client->addr,
                        .flags = I2C_M_RD,
                        .len = length,
                        .buf = rxData,
                },
        };

        for (retry = 0; retry < 3; retry++)
        {
                if (i2c_transfer(this_client->adapter, msgs, 2) > 0)
                        break;
                else
                        mdelay(10);
        }

        if (retry >= 3)
        {
                printk(KERN_ERR "%s: retry over 3\n", __func__);
                return -EIO;
        }
        else
                return 0;
}

static int VTC_i2c_Tx(char *txData, int length)
{
        int retry;
        struct i2c_msg msg[] =
        {
                {
                        .addr = this_client->addr,
                        .flags = 0,
                        .len = length,
                        .buf = txData,
                },
        };

        for (retry = 0; retry <= 3; retry++)
        {
                if (i2c_transfer(this_client->adapter, msg, 1) > 0)
                        break;
                else
                        mdelay(10);
        }

        if (retry > 3)
        {
                printk(KERN_ERR "%s: retry over 3\n", __func__);
                return -EIO;
        }
        else
                return 0;
}
/*----------------------------------------------------------------------------*/
static int af6133_mag_get_data(struct af6133_data *mag)
{
  int err;
  u8  data[6];
  int i;
  int xyz[3];
  int output[3] = {0};

  data[0] = REG_DATA2;

 err = VTC_i2c_Rx(data, 6);
  if(err < 0) goto err_i2c_fail;

  xyz[0] = data[0] | (data[1] << 8);
  xyz[1] = data[2] | (data[3] << 8);
  xyz[2] = data[4] | (data[5] << 8);

  // printk(KERN_ERR"HJDDbgMag 3, xyz[0]=%d, xyz[1]=%d, xyz[2]=%d \n", xyz[0], xyz[1], xyz[2]);

  for(i=0;i<3;i++)
  {
    xyz[i] = (xyz[i] > 32767) ? (xyz[i] - 65536) : xyz[i];
    // printk(KERN_ERR"HJDDbgMag 1, xyz[%d]=%d \n", i, xyz[i]);

    //reduce sensor offset
    xyz[i] -= mag->pdata->offset[i];
    // printk(KERN_ERR"HJDDbgMag 2, xyz[%d]=%d \n", i, xyz[i]);

    //digital gain
    xyz[i] = xyz[i] * mag->pdata->gain[i] / 100;
    // printk(KERN_ERR"HJDDbgMag 3, xyz[%d]=%d \n", i, xyz[i]);
  }

  output[0] = xyz[0] + (xyz[1] * mag->pdata->comp[0]) / 100;
  output[1] = xyz[1] + (xyz[0] * mag->pdata->comp[1]) / 100;
  output[2] = xyz[2] + (xyz[0] * mag->pdata->comp[2]) / 100 +
                       (xyz[1] * mag->pdata->comp[3]) / 100;
  // printk(KERN_ERR"HJDDbgMag 3, output[0]=%d, output[1]=%d, output[2]=%d \n", output[0], output[1], output[2]);

  //next read
  data[0] = 0x0A;
  data[1] = 0x01;
  err = VTC_i2c_Tx(data, 2);

  if(err < 0) goto err_i2c_fail;

  input_report_abs(mag->input, ABS_X, output[0]);
  input_report_abs(mag->input, ABS_Y, output[1]);
  input_report_abs(mag->input, ABS_Z, output[2]);
  input_sync(mag->input);

  return 0;

err_i2c_fail:
  printk(KERN_ERR "%s:failed\n", __func__);
  return err;
}
/*----------------------------------------------------------------------------*/
static int af6133_SetMeasurementReg(void)
{
  u8 data[2];
  int err;

  data[0] = REG_RANGE;
  data[1] = 0x32;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_AVG;
  data[1] = 0x33;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_XY_SR;
  data[1] = 0x05;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_OSR;
  data[1] = 0x00;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_WAITING;
  data[1] = 0x00;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_Z_SR;
  data[1] = 0x24;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_TEST0;
  data[1] = 0x83;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_TEST2;
  data[1] = 0x00;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_MEASURE;
  data[1] = 0x01;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  mdelay(2);

  return 0;
}
/*----------------------------------------------------------------------------*/
static int af6133_GetSensorOffset(int16_t *offset)
{
  /* AF6133 B2 SET/RESET & Offset Cancelation (1/3) */
  uint8_t data[6];
  uint8_t i;
  int16_t S_Data[3], R_Data[3];
  int16_t err;
  int32_t tmp[3];

  for(i=0;i<3;i++)
    tmp[i] = 0;

  /* set reading */
  data[0] = REG_XY_SR;
  data[1] = 0x08;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_Z_SR;
  data[1] = 0x44;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  for(i=0;i<MAG_OFFSET_LOOP;i++)
  {
    data[0] = REG_MEASURE;
    data[1] = 0x01;
    err = VTC_i2c_Tx(data, 2);
    if(err < 0) return -1;
    mdelay(4);

    data[0] = REG_DATA;
    err = VTC_i2c_Rx(data, 6);
    if(err < 0) return -1;

    S_Data[0] = (data[1] << 8) | data[0];
    S_Data[1] = (data[3] << 8) | data[2];
    S_Data[2] = (data[5] << 8) | data[4];

    tmp[0] += (int32_t)(S_Data[0]);
    tmp[1] += (int32_t)(S_Data[1]);
    tmp[2] += (int32_t)(S_Data[2]);
  }

  /* reset reading */
  data[0] = REG_XY_SR;
  data[1] = 0x04;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_Z_SR;
  data[1] = 0x24;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  for(i=0;i<MAG_OFFSET_LOOP;i++)
  {
    data[0] = REG_MEASURE;
    data[1] = 0x01;
    err = VTC_i2c_Tx(data, 2);
    if(err < 0) return -1;
    mdelay(4);

    data[0] = REG_DATA2;
    err = VTC_i2c_Rx(data, 6);
    if(err < 0) return -1;

    R_Data[0] = (data[1] << 8) | data[0];
    R_Data[1] = (data[3] << 8) | data[2];
    R_Data[2] = (data[5] << 8) | data[4];

    tmp[0] += (int32_t)(R_Data[0]);
    tmp[1] += (int32_t)(R_Data[1]);
    tmp[2] += (int32_t)(R_Data[2]);
  }

  for(i=0;i<3;i++)
  {
    offset[i] = (int16_t)(tmp[i] / (MAG_OFFSET_LOOP*2));
  }
  /* george modify begin 20180205 */
  printk(KERN_ERR"VTC_Debug_Mag af6133_GetSensorOffset, offset[0]=%d, offset[1]=%d, offset[2]=%d \n", offset[0], offset[1], offset[2]);
  /* george modify end   20180205 */

  return 0;
}
/*----------------------------------------------------------------------------*/
static int af6133_otp_set_offset(int16_t *index)
{
  /* AF6133 B2 SET/RESET & Offset Cancelation (2/3) */
  int16_t i;
  uint8_t data[2];
  uint8_t otp;
  uint8_t flag;
  int16_t err;

  /* OTP write setting */
  data[0] = REG_TEST0;
  data[1] = 0xA3;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_SECURITY;
  data[1] = 0x56;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_SECURITY;
  data[1] = 0x54;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_SECURITY;
  data[1] = 0x43;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_SECURITY;
  data[1] = 0xAA;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  /* set offset OTP */
  for(i=0;i<3;i++)
  {
    if(index[i] < 0)
    {
      flag = 1;
      index[i] = -index[i];
    }
    else
    {
      flag = 0;
    }

    otp = (((uint8_t)(index[i])) & 0x0F) | (flag ? 0x10 : 0x00);

    /* george modify begin 20180205 */
    printk(KERN_ERR"VTC_Debug_Mag af6133_otp_set_offset, REG_OTP:0x%x  otp=%d \n", REG_OTP_1D + i, otp);
    /* george modify end   20180205 */

    data[0] = REG_OTP_1D + i;
    data[1] = otp;
    err = VTC_i2c_Tx(data, 2);
    if(err < 0) return -1;
  }

  data[0] = REG_TEST0;
  data[1] = 0x83;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  return 0;
}
/*----------------------------------------------------------------------------*/
static int af6133_SetSensorOffset(struct af6133_data *mag)
{
  int16_t i;
  int16_t index[3];
  int16_t offset[3];

  /* george modify begin 20180205 */
  printk(KERN_ERR"VTC_Debug_Mag af6133_SetSensorOffset:: get sensor offset 1 \n");
  /* george modify end   20180205 */
  /* get sensor offset 1 */
  if(af6133_GetSensorOffset(offset))
  {
    return -1;
  }

  for(i=0;i<3;i++)
  {
    index[i] = offset[i] / MAG_MIN_OFFSET;

    if(index[i] % 2)
      index[i] += ((index[i]>0) ? 1 : -1);

    index[i] /= 2;
  }

  /* george modify begin 20180205 */
  printk(KERN_ERR"VTC_Debug_Mag af6133_SetSensorOffset:: Entry af6133_otp_set_offset \n");
  /* george modify end   20180205 */
  if(index[0] || index[1] || index[2])
  {
    // reduce sensor offset 1
    if(af6133_otp_set_offset(index))
    {
      return -1;
    }

    /* george modify begin 20180205 */
    printk(KERN_ERR"VTC_Debug_Mag af6133_SetSensorOffset:: get sensor offset 2 \n");
    /* george modify end   20180205 */
    /* get sensor offset 2 */
    if(af6133_GetSensorOffset(offset))
    {
      return -1;
    }
  }

  /* george modify begin 20180205 */
  printk(KERN_ERR"VTC_Debug_Mag af6133_SetSensorOffset::  offset[0]=%d, offset[1]=%d, offset[2]=%d \n", offset[0], offset[1], offset[2]);
  /* george modify end   20180205 */
  /* update sensor offset */
  for(i=0;i<3;i++)
  {
    mag->pdata->offset[i] = offset[i];
  }

  return 0;
}
/*----------------------------------------------------------------------------*/
static int af6133_GetBistData(int16_t *bist_X, int16_t *bist_Y, int16_t *bist_Z)
{
  /* AF6133 B2 BIST for Sensitivity & Orthogonality (1/3) */
  int16_t i;
  int16_t mag_P[3], mag_N[3];
  uint8_t data[6];
  int32_t tmp[3];
  int16_t err;

  data[0] = REG_XY_SR;
  data[1] = 0x04;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_Z_SR;
  data[1] = 0x24;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  /* Measure BIST X */
  for(i=0;i<3;i++)
    tmp[i] = 0;

  for(i=0;i<MAG_BIST_LOOP;i++)
  {
    data[0] = REG_MEASURE;
    data[1] = 0x45;
    err = VTC_i2c_Tx(data, 2);
    if(err < 0) return -1;
    mdelay(8);

    data[0] = REG_DATA;
    err = VTC_i2c_Rx(data, 6);
    if(err < 0) return -1;

    mag_P[0] = (data[1] << 8) | data[0];
    mag_P[1] = (data[3] << 8) | data[2];
    mag_P[2] = (data[5] << 8) | data[4];

    data[0] = REG_DATA2;
    err = VTC_i2c_Rx(data, 6);
    if(err < 0) return -1;

    mag_N[0] = (data[1] << 8) | data[0];
    mag_N[1] = (data[3] << 8) | data[2];
    mag_N[2] = (data[5] << 8) | data[4];

    tmp[0] += (int32_t)(mag_P[0] - mag_N[0]);
    tmp[1] += (int32_t)(mag_N[1] - mag_P[1]);
    tmp[2] += (int32_t)(mag_P[2] - mag_N[2]);
  }

  for(i=0;i<3;i++)
    bist_X[i] = -(int16_t)(tmp[i] / MAG_BIST_LOOP);

  /* Measure BIST Y */
  for(i=0;i<3;i++)
    tmp[i] = 0;

  for(i=0;i<MAG_BIST_LOOP;i++)
  {
    data[0] = REG_MEASURE;
    data[1] = 0x49;
    err = VTC_i2c_Tx(data, 2);
    if(err < 0) return -1;
    mdelay(8);

    data[0] = REG_DATA;
    err = VTC_i2c_Rx(data, 6);
    if(err < 0) return -1;

    mag_P[0] = (data[1] << 8) | data[0];
    mag_P[1] = (data[3] << 8) | data[2];
    mag_P[2] = (data[5] << 8) | data[4];

    data[0] = REG_DATA2;
    err = VTC_i2c_Rx(data, 6);
    if(err < 0) return -1;

    mag_N[0] = (data[1] << 8) | data[0];
    mag_N[1] = (data[3] << 8) | data[2];
    mag_N[2] = (data[5] << 8) | data[4];

    tmp[0] += (int32_t)(mag_P[0] - mag_N[0]);
    tmp[1] += (int32_t)(mag_N[1] - mag_P[1]);
    tmp[2] += (int32_t)(mag_N[2] - mag_P[2]);
  }

  for(i=0;i<3;i++)
    bist_Y[i] = -(int16_t)(tmp[i] / MAG_BIST_LOOP);

  /* Measure BIST Z */
  for(i=0;i<3;i++)
    tmp[i] = 0;

  for(i=0;i<MAG_BIST_LOOP;i++)
  {
    data[0] = REG_MEASURE;
    data[1] = 0x4D;
    err = VTC_i2c_Tx(data, 2);
    if(err < 0) return -1;
    mdelay(8);

    data[0] = REG_DATA;
    err = VTC_i2c_Rx(data, 6);
    if(err < 0) return -1;

    mag_P[0] = (data[1] << 8) | data[0];
    mag_P[1] = (data[3] << 8) | data[2];
    mag_P[2] = (data[5] << 8) | data[4];

    data[0] = REG_DATA2;
    err = VTC_i2c_Rx(data, 6);
    if(err < 0) return -1;

    mag_N[0] = (data[1] << 8) | data[0];
    mag_N[1] = (data[3] << 8) | data[2];
    mag_N[2] = (data[5] << 8) | data[4];

    tmp[0] += (int32_t)(mag_P[0] - mag_N[0]);
    tmp[1] += (int32_t)(mag_P[1] - mag_N[1]);
    tmp[2] += (int32_t)(mag_P[2] - mag_N[2]);
  }

  for(i=0;i<3;i++)
    bist_Z[i] = -(int16_t)(tmp[i] / MAG_BIST_LOOP);

  /* Recovery Golden Mode */
  data[0] = REG_XY_SR;
  data[1] = 0x05;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;

  data[0] = REG_MEASURE;
  data[1] = 0x01;
  err = VTC_i2c_Tx(data, 2);
  if(err < 0) return -1;
  mdelay(2);

  return 0;
}
/*----------------------------------------------------------------------------*/
static int af6133_SetBistData(struct af6133_data *mag)
{
  int16_t i;
  int16_t bistX[3], bistY[3], bistZ[3];

  /* get BIST data */
  if(af6133_GetBistData(bistX, bistY, bistZ))
  {
    return -1;
  }

  /* update BIST data */
  for(i=0;i<3;i++)
  {
    mag->pdata->bist_x[i] = bistX[i];
    mag->pdata->bist_y[i] = bistY[i];
    mag->pdata->bist_z[i] = bistZ[i];
  }

  /* update senosr gain */
  mag->pdata->gain[0] = (int16_t)(BIST_GAIN_COEFF_X / bistX[0]);
  mag->pdata->gain[1] = (int16_t)(BIST_GAIN_COEFF_Y / bistY[1]);
  mag->pdata->gain[2] = (int16_t)(BIST_GAIN_COEFF_Z / bistZ[2]);

  /* update sens. comp. */
  mag->pdata->comp[0] = -(BIST_COMP_COEFF_0 * bistY[0] / bistX[0] + BIST_COMP_BIAS_0);
  mag->pdata->comp[1] = -(BIST_COMP_COEFF_1 * bistX[1] / bistY[1] + BIST_COMP_BIAS_1);
  mag->pdata->comp[2] = -(BIST_COMP_COEFF_2 * bistX[2] / bistZ[2] + BIST_COMP_BIAS_2);
  mag->pdata->comp[3] = -(BIST_COMP_COEFF_3 * bistY[2] / bistZ[2] + BIST_COMP_BIAS_3);

  /* george modify begin 20180205 */
  printk(KERN_ERR"VTC_Debug_Mag af6133_SetBistData::  bistX[0]=%d, bistX[1]=%d, bistX[2]=%d \n",
  mag->pdata->bist_x[0],
  mag->pdata->bist_x[1],
  mag->pdata->bist_x[2]);
  printk(KERN_ERR"VTC_Debug_Mag af6133_SetBistData::  bistY[0]=%d, bistY[1]=%d, bistY[2]=%d \n",
  mag->pdata->bist_y[0],
  mag->pdata->bist_y[1],
  mag->pdata->bist_y[2]);
  printk(KERN_ERR"VTC_Debug_Mag af6133_SetBistData::  bistZ[0]=%d, bistZ[1]=%d, bistZ[2]=%d \n",
  mag->pdata->bist_z[0],
  mag->pdata->bist_z[1],
  mag->pdata->bist_z[2]);
  printk(KERN_ERR"VTC_Debug_Mag af6133_SetBistData::  gain[0]=%d, gain[1]=%d, gain[2]=%d \n",
  mag->pdata->gain[0],
  mag->pdata->gain[1],
  mag->pdata->gain[2]);
  printk(KERN_ERR"VTC_Debug_Mag af6133_SetBistData::  comp[0]=%d, comp[1]=%d, comp[2]=%d, comp[3]=%d \n",
  mag->pdata->comp[0],
  mag->pdata->comp[1],
  mag->pdata->comp[2],
  mag->pdata->comp[3]);
  /* george modify end   20180205 */

  return 0;
}
/*----------------------------------------------------------------------------*/
void af6133_Data_Initial(struct af6133_data *mag)
{
  int16_t i;

  for(i=0;i<3;i++)
  {
    mag->pdata->offset[i] = 0;
    mag->pdata->gain[i] = 100;
  }

  for(i=0;i<4;i++)
    mag->pdata->comp[i] = 0;
}
/*----------------------------------------------------------------------------*/
static int af6133_i2c_init(struct af6133_data *mag)
{
  int err;
  u8  data[2];

  //*********************************
  //initial af6133_platform_data
  //*********************************
  af6133_Data_Initial(mag);

  //*********************************
  //Check PID
  //*********************************
  data[0] = REG_PCODE;
  err = VTC_i2c_Rx(data, 1);
  if(err < 0) return -1;

  if(AF6133J_PID != data[0])
  {
    printk(KERN_ERR "%s: PID = %d, PID failed\n", __func__, data[0]);
    return -2;
  }

  //*********************************
  //set measurement settings
  //*********************************
  err = af6133_SetMeasurementReg();
  if(err)
  {
    printk(KERN_ERR "%s: af6133_SetMeasurementReg failed\n", __func__);
    return err;
  }

  //*********************************
  //set sensor offset
  //*********************************
  err = af6133_SetSensorOffset(mag);
  if(err)
  {
    printk(KERN_ERR "%s: af6133_SetSensorOffset failed\n", __func__);
    return err;
  }

  //*********************************
  //set BIST data
  //*********************************
  err = af6133_SetBistData(mag);
  if(err)
  {
    printk(KERN_ERR "%s: af6133_SetBistData failed\n", __func__);
    return err;
  }

  return 0;
}

static ssize_t attr_get_polling_rate(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
        struct af6133_data *mag = dev_get_drvdata(dev);
        unsigned long interval_ms;
        mutex_lock(&mag->lock);
        interval_ms = mag->pdata->poll_interval;
        mutex_unlock(&mag->lock);
	return sprintf(buf, "%ld\n", interval_ms);
}

static ssize_t attr_set_polling_rate(struct device *dev,
                                     struct device_attribute *attr,
                                     const char *buf, size_t size)
{
        struct af6133_data *mag = dev_get_drvdata(dev);
        unsigned long interval_ms;

        if (kstrtoul(buf, 10, &interval_ms))
                return -EINVAL;
        if (!interval_ms)
                return -EINVAL;

        if(interval_ms > DELAY_TIME_MAX)
        	interval_ms = DELAY_TIME_MAX;
        else if(interval_ms < DELAY_TIME_MIN)
        	interval_ms = DELAY_TIME_MIN;

        mutex_lock(&mag->lock);
        mag->pdata->poll_interval = interval_ms;
        mutex_unlock(&mag->lock);

        if(atomic_read(&mag->enabled))
         schedule_delayed_work(&mag->work, msecs_to_jiffies(mag->pdata->poll_interval));

        return size;
}

static ssize_t attr_get_enable(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
        struct af6133_data *mag = dev_get_drvdata(dev);
        int val = atomic_read(&mag->enabled);
        return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable(struct device *dev,
                               struct device_attribute *attr,
                               const char *buf, size_t size)
{
       struct af6133_data *mag = dev_get_drvdata(dev);
       unsigned int pre_en = atomic_read(&mag->enabled);
       unsigned int new_en = 0;

        //if (strict_strtoul(buf, 10, &new_en))
        //  return -EINVAL;
        if (sysfs_streq(buf, "1"))
                new_en = 1;
        else if (sysfs_streq(buf, "0"))
                new_en = 0;

        if (new_en != pre_en)
        {
         if(new_en)
          {
           schedule_delayed_work(&mag->work, msecs_to_jiffies(mag->pdata->poll_interval));
           atomic_set(&mag->enabled, 1);
          }
         else
          {
           cancel_delayed_work_sync(&mag->work);
           atomic_set(&mag->enabled, 0);
          }
        }

        return size;
}


static struct device_attribute attributes[] = {
        __ATTR(pollrate_ms, 0660, attr_get_polling_rate, attr_set_polling_rate),
        __ATTR(enable_device, 0660, attr_get_enable, attr_set_enable),
};

static int create_device_attributes(
	struct device *dev,
	struct device_attribute *attrs)
{
	int i;
	int err = 0;

	for (i = 0 ; NULL != attrs[i].attr.name ; ++i) {
		err = device_create_file(dev, &attrs[i]);
		if (err)
			break;
	}

	if (err) {
		for (--i; i >= 0 ; --i)
			device_remove_file(dev, &attrs[i]);
	}

	return err;
}

static char const *const device_link_name = "i2c";
static dev_t const af6133_device_dev_t = MKDEV(MISC_MAJOR, 240);

static int create_sysfs_interfaces(struct af6133_data *af_mag)
{
        int err = 0;

        if (NULL == af_mag)
          return -EINVAL;

        af_mag->compass = class_create(THIS_MODULE, AF6133_SYS_CLS_NAME);
	if(IS_ERR(af_mag->compass))
	{
                err = PTR_ERR(af_mag->compass);
	        printk("%s, create class, error\n", __func__);
		return err;
	}

	af_mag->class_dev = device_create(af_mag->compass,
                                          NULL,
                                          af6133_device_dev_t,
                                          af_mag,
                                          AF6133_SYS_DEV_NAME);
	if (IS_ERR(af_mag->class_dev)) {
		err = PTR_ERR(af_mag->class_dev);
                printk("%s, create class device, error\n", __func__);
		return err;
	}

	err = create_device_attributes(af_mag->class_dev, attributes);
	if (0 > err) {
                printk("%s, create attributes, error\n", __func__);
        }

	return err;
}

static int remove_sysfs_interfaces(struct device *dev)
{
        int i;
        for (i = 0; i < ARRAY_SIZE(attributes); i++)
                device_remove_file(dev, attributes + i);
        return 0;
}

static void af6133_work_func(struct work_struct *work)
{
        struct af6133_data *mag = container_of((struct delayed_work *)work, struct af6133_data, work);
        int err = 0;

        printk(KERN_ERR"HJDDbgMag, af6133_work_func");
        err = af6133_mag_get_data(mag);
        if (err < 0)
          dev_err(&mag->client->dev, "get magnetometer data failed\n");

       schedule_delayed_work(&mag->work, msecs_to_jiffies(mag->pdata->poll_interval));
}

static int af6133_mag_input_init(struct af6133_data *mag)
{
       int err;
       struct input_dev *dev;

        dev = input_allocate_device();

        if (!dev) {
          return -ENOMEM;
        }

        dev->id.bustype = BUS_I2C;
        dev->name = AF6133_INPUT_DEV_NAME;

        set_bit(EV_ABS, dev->evbit);
        input_set_abs_params(dev, ABS_X, -32768, 32767, 0, 0);
        input_set_abs_params(dev, ABS_Y, -32768, 32767, 0, 0);
        input_set_abs_params(dev, ABS_Z, -32768, 32767, 0, 0);

       input_set_drvdata(dev, mag);

        err = input_register_device(dev);
       if (err < 0) {
         input_free_device(dev);
         return err;
        }

       mag->input = dev;

        return 0;
}

static int af6133_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        struct af6133_data *mag;
        int err;

        pr_info("%s: driver version is %s\n", __func__, AF6133_DRIVER_VERSION);

        if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
        {
                dev_err(&client->dev, "i2c_check_functionality error\n");
                err = -ENODEV;
                goto err_check_func;
        }

				/* Allocate memory for driver data */
        mag = kzalloc(sizeof(struct af6133_data), GFP_KERNEL);
        if (!mag) {
                dev_err(&client->dev, "failed to allocate memory for module data\n");
                err = -ENOMEM;
                goto err_alloc_mag_data;
        }

        mutex_init(&mag->lock);
        mutex_lock(&mag->lock);
        mag->client = client;

        mag->pdata = kmalloc(sizeof(*mag->pdata), GFP_KERNEL);
        if (!mag->pdata)
                goto err_alloc_mag_pdata;

	/***** I2C initialization *****/
        i2c_set_clientdata(client, mag);
        this_client = mag->client;

        err = af6133_i2c_init(mag);
        if (err < 0)
          goto err_alloc_mag_pdata;

	/***** input *****/
        err = af6133_mag_input_init(mag);
        if (err < 0)
          goto err_input_init;

	/***** sysfs *****/
        err = create_sysfs_interfaces(mag);
        if (err < 0) {
                dev_err(&client->dev, "%s create sysfs interfacce failed\n", AF6133_I2C_DEV_NAME);
                goto err_clean_input;
        }

    	/* workqueue init */
        printk(KERN_ERR"HJDDbgMag, INIT_DELAYED_WORK--af6133_work_func");

        mag->pdata->poll_interval = 10; // Jiangde, default

        INIT_DELAYED_WORK(&mag->work, af6133_work_func);

        mutex_unlock(&mag->lock);
        dev_info(&client->dev, "af6133 probed successfully\n");

        atomic_set(&mag->enabled, 0);

        return 0;

err_clean_input:
        input_unregister_device(mag->input);
err_input_init:
        kfree(mag->pdata);
err_alloc_mag_pdata:
        kfree(mag);
err_alloc_mag_data:
err_check_func:
        pr_err("%s: AF6133 Driver Initialization failed\n", __func__);
        i2c_set_clientdata(client, NULL);
        return err;
}

static int af6133_remove(struct i2c_client *client)
{
        struct af6133_data *mag = i2c_get_clientdata(client);

        if( mag == NULL )
                return 0;

       pr_info("%s: af6133 remove\n", __func__);

        remove_sysfs_interfaces(mag->class_dev);
       input_unregister_device(mag->input);

       kfree(mag->pdata);
        kfree(mag);

        return 0;
}

static int af6133_suspend(struct device *dev)
{
       struct af6133_data *mag = dev_get_drvdata(dev);

        if( mag == NULL )
                return 0;

       if (atomic_read(&mag->enabled))
         cancel_delayed_work_sync(&mag->work);

       pr_info("%s: af6133 suspend\n", __func__);
        return 0;
}

static int af6133_resume(struct device *dev)
{
        struct af6133_data *mag = dev_get_drvdata(dev);

        if( mag == NULL )
                return 0;

        af6133_SetMeasurementReg();

        if (atomic_read(&mag->enabled))
          schedule_delayed_work(&mag->work, msecs_to_jiffies(mag->pdata->poll_interval));

        pr_info("%s: af6133 resume\n", __func__);
        return 0;
}

static const struct i2c_device_id af6133_i2c_id[] = {
        {AF6133_I2C_DEV_NAME, 0},
        {},
};

//MODULE_DEVICE_TABLE(i2c, af6133_mag_id);

static struct dev_pm_ops af6133_pm = {
        .suspend = af6133_suspend,
        .resume = af6133_resume,
};

static struct of_device_id af6133_match_table[] = {
	{ .compatible = "vtc,af6133", },
	{},
};

static struct i2c_driver af6133_driver = {
        .driver = {
                        .owner = THIS_MODULE,
                        .name = AF6133_I2C_DEV_NAME,
			.of_match_table = af6133_match_table,
                        .pm = &af6133_pm,
            },
        .probe = af6133_probe,
        .remove = af6133_remove,
        .id_table = af6133_i2c_id,
};

//static struct i2c_board_info __initdata i2c_af6133={ I2C_BOARD_INFO(AF6133_I2C_DEV_NAME, 0x0C)};
static int __init af6133_init(void)
{
        pr_info("%s: af6133 3-axis megnetometer driver: init\n", __func__);

        //struct i2c_adapter *adapter;
        //struct i2c_client *client;

        //adapter = i2c_get_adapter(2);
        //client = i2c_new_device(adapter, &i2c_af6133);

        return i2c_add_driver(&af6133_driver);
}

static void __exit af6133_exit(void)
{
        pr_info("%s: af6133 3-axis megnetometer driver: exit\n", __func__);
        i2c_del_driver(&af6133_driver);
        return;
}

module_init(af6133_init);
module_exit(af6133_exit);

MODULE_DESCRIPTION("af6133 3-axis magnetometer driver");
MODULE_AUTHOR("Voltafield CAE team");
MODULE_LICENSE("GPL");
MODULE_VERSION(AF6133_DRIVER_VERSION);
