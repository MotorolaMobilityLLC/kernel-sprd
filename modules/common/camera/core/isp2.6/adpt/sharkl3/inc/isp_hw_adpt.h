/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ISP_HW_ADPT_H_
#define _ISP_HW_ADPT_H_

/* configure fmcu with isp register offset, range is 0x60b-0x60c */
#define ISP_OFFSET_RANGE                        0x60c060b

#define ISP_WIDTH_MAX                           4672
#define ISP_HEIGHT_MAX                          3504

#define ISP_SCALER_UP_MAX                       4

#define ISP_SC_COEFF_COEF_SIZE                  (1 << 10)
#define ISP_SC_COEFF_TMP_SIZE                   (21 << 10)

#define ISP_SC_H_COEF_SIZE                      0xC0
#define ISP_SC_V_COEF_SIZE                      0x210
#define ISP_SC_V_CHROM_COEF_SIZE                0x210

#define ISP_SC_COEFF_H_NUM                      (ISP_SC_H_COEF_SIZE / 6)
#define ISP_SC_COEFF_H_CHROMA_NUM               (ISP_SC_H_COEF_SIZE / 12)
#define ISP_SC_COEFF_V_NUM                      (ISP_SC_V_COEF_SIZE / 4)
#define ISP_SC_COEFF_V_CHROMA_NUM               (ISP_SC_V_CHROM_COEF_SIZE / 4)

#define ISP_SC_COEFF_BUF_SIZE                   (24 << 10)

#define ISP_FBD_TILE_WIDTH                      64
#define ISP_FBD_TILE_HEIGHT                     4

#define ISP_LTM_ALIGNMENT                       2
#endif
