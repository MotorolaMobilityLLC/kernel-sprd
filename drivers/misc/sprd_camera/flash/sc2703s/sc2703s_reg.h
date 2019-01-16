/*
 * Copyright (C) 2015-2016 Spreadtrum Communications Inc.
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
#ifndef _SC2703S_FLASH_REG_H_
#define _SC2703S_FLASH_REG_H_

/* Registers */

#define SC2703_FLASH_EVENT                     0x00
#define SC2703_FLASH_STATUS                    0x01
#define SC2703_FLASH_IRQ_MASK                  0x02
#define SC2703_FLASH_DRIVER_ACTIVE             0x03
#define SC2703_FLASH_DRIVER_STATUS             0x04
#define SC2703_FLASH_FD_CONFIG1                0x05
#define SC2703_FLASH_FD_CONFIG2                0x06
#define SC2703_FLASH_FD_CONFIG3                0x07
#define SC2703_FLASH_FD_CONFIG4                0x08
#define SC2703_FLASH_FD_CONFIG5                0x09
#define SC2703_FLASH_FD_CONFIG6                0x0a
#define SC2703_FLASH_FD_CONFIG7                0x0b
#define SC2703_FLASH_FD_CONFIG8                0x0c
#define SC2703_FLASH_FD_CONFIG9                0x0d
#define SC2703_FLASH_FD_CONFIG10               0x0e
#define SC2703_FLASH_FD_FAULT_CONFIG           0x0f

/* SC2703_FLASH_EVENT = 0x00 */
#define SC2703_EVT_CH1_OC_SHIFT                      0
#define SC2703_EVT_CH1_OC_MASK                       BIT(0)
#define SC2703_EVT_CH1_SC_SHIFT                      1
#define SC2703_EVT_CH1_SC_MASK                       BIT(1)
#define SC2703_EVT_CH2_OC_SHIFT                      2
#define SC2703_EVT_CH2_OC_MASK                       BIT(2)
#define SC2703_EVT_CH2_SC_SHIFT                      3
#define SC2703_EVT_CH2_SC_MASK                       BIT(3)
#define SC2703_EVT_OT_SHIFT                          4
#define SC2703_EVT_OT_MASK                           BIT(4)
#define SC2703_EVT_FAULT_CLEAR_EN_SHIFT              5
#define SC2703_EVT_FAULT_CLEAR_EN_MASK               BIT(5)
#define SC2703_EVT_TARGET_EXCEED_MAX_SHIFT           6
#define SC2703_EVT_TARGET_EXCEED_MAX_MASK            BIT(6)
#define SC2703_EVT_VCENT_DROP_SHIFT                  7
#define SC2703_EVT_VCENT_DROP_MASK                   BIT(7)
/* SC2703_FLASH_STATUS = 0x01 */
#define SC2703_CH1_OC_SHIFT                          0
#define SC2703_CH1_OC_MASK                           BIT(0)
#define SC2703_CH1_SC_SHIFT                          1
#define SC2703_CH1_SC_MASK                           BIT(1)
#define SC2703_CH2_OC_SHIFT                          2
#define SC2703_CH2_OC_MASK                           BIT(2)
#define SC2703_CH2_SC_SHIFT                          3
#define SC2703_CH2_SC_MASK                           BIT(3)
#define SC2703_OT_SHIFT                              4
#define SC2703_OT_MASK                               BIT(4)
#define SC2703_FAULT_CLEAR_EN_SHIFT                  5
#define SC2703_FAULT_CLEAR_EN_MASK                   BIT(5)
#define SC2703_TARGET_EXCEED_MAX_SHIFT               6
#define SC2703_TARGET_EXCEED_MAX_MASK                BIT(6)
#define SC2703_VCENT_DROP_SHIFT                      7
#define SC2703_VCENT_DROP_MASK                       BIT(7)
/* SC2703_FLASH_IRQ_MASK = 0x02 */
#define SC2703_M_CH1_OC_SHIFT                        0
#define SC2703_M_CH1_OC_MASK                         BIT(0)
#define SC2703_M_CH1_SC_SHIFT                        1
#define SC2703_M_CH1_SC_MASK                         BIT(1)
#define SC2703_M_CH2_OC_SHIFT                        2
#define SC2703_M_CH2_OC_MASK                         BIT(2)
#define SC2703_M_CH2_SC_SHIFT                        3
#define SC2703_M_CH2_SC_MASK                         BIT(3)
#define SC2703_M_OT_SHIFT                            4
#define SC2703_M_OT_MASK                             BIT(4)
#define SC2703_M_FAULT_CLEAR_EN_SHIFT                5
#define SC2703_M_FAULT_CLEAR_EN_MASK                 BIT(5)
#define SC2703_M_TARGET_EXCEED_MAX_SHIFT             6
#define SC2703_M_TARGET_EXCEED_MAX_MASK              BIT(6)
#define SC2703_M_VCENT_DROP_SHIFT                    7
#define SC2703_M_VCENT_DROP_MASK                     BIT(7)
/* SC2703_FLASH_DRIVER_ACTIVE = 0x03 */
#define SC2703_LED1_EN_SHIFT                         0
#define SC2703_LED1_EN_MASK                          BIT(0)
#define SC2703_LED2_EN_SHIFT                         1
#define SC2703_LED2_EN_MASK                          BIT(1)
#define SC2703_LED_MODE_SHIFT                            2
#define SC2703_LED_MODE_MASK                             BIT(2)
#define SC2703_LED1_DRIVER_EN_SHIFT                  4
#define SC2703_LED1_DRIVER_EN_MASK                   BIT(4)
#define SC2703_LED2_DRIVER_EN_SHIFT                  5
#define SC2703_LED2_DRIVER_EN_MASK                   BIT(5)
#define SC2703_LED1_PIN_SEL_DRIVER_EN_SHIFT          6
#define SC2703_LED1_PIN_SEL_DRIVER_EN_MASK           BIT(6)
#define SC2703_LED2_PIN_SEL_DRIVER_EN_SHIFT          7
#define SC2703_LED2_PIN_SEL_DRIVER_EN_MASK           BIT(7)
/* SC2703_FLASH_DRIVER_STATUS = 0x04 */
#define SC2703_DRIVER_IS_ACTIVE_SHIFT                0
#define SC2703_DRIVER_IS_ACTIVE_MASK                 (0x3 << 0)
#define SC2703_DRIVER_IS_ACTIVE_NONE		(0x0)
#define SC2703_DRIVER_IS_ACTIVE_STANDBY		(0x1)
#define SC2703_DRIVER_IS_ACTIVE_TORCHING	(0x2)
#define SC2703_DRIVER_IS_ACTIVE_FLASHING	(0x3)
#define SC2703_DRIVER_VIN_IS_PG_SHIFT                4
#define SC2703_DRIVER_VIN_IS_PG_MASK                 BIT(4)
/* SC2703_FLASH_FD_CONFIG1 = 0x05 */
#define SC2703_WARMUP_TIME_SEL_SHIFT                 0
#define SC2703_WARMUP_TIME_SEL_MASK                  (0x3 << 0)
#define SC2703_BRIGHT_WIDTH_FLASH_SHIFT              2
#define SC2703_BRIGHT_WIDTH_FLASH_MASK               (0x7 << 2)
#define SC2703_BRIGHT_WIDTH_TORCH_SHIFT              5
#define SC2703_BRIGHT_WIDTH_TORCH_MASK               (0x7 << 5)
/* SC2703_FLASH_FD_CONFIG2 = 0x06 */
#define SC2703_RAMPDOWN_DISABLE_SHIFT                0
#define SC2703_RAMPDOWN_DISABLE_MASK                 (0x7 << 0)
#define SC2703_RAMPDOWN_PROTECT_SHIFT                3
#define SC2703_RAMPDOWN_PROTECT_MASK                 (0x7 << 3)
#define SC2703_DEG_PROTECT_SHIFT                     6
#define SC2703_DEG_PROTECT_MASK                      (0x3 << 6)
/* SC2703_FLASH_FD_CONFIG3 = 0x07 */
#define SC2703_FLASH_TIMEOUT_SHIFT                   0
#define SC2703_FLASH_TIMEOUT_MASK                    (0xF << 0)
#define SC2703_FD_EN_PREC_SHIFT                      4
#define SC2703_FD_EN_PREC_MASK                       BIT(4)
#define SC2703_DISABLE_FLASH_TIMEOUT_SHIFT           5
#define SC2703_DISABLE_FLASH_TIMEOUT_MASK            BIT(5)
/* SC2703_FLASH_FD_CONFIG4 = 0x08 */
#define SC2703_TRANSEL_FLASH_SHIFT                   0
#define SC2703_TRANSEL_FLASH_MASK                    (0xF << 0)
#define SC2703_TRANSEL_TORCH_SHIFT                   4
#define SC2703_TRANSEL_TORCH_MASK                    (0xF << 4)
/* SC2703_FLASH_FD_CONFIG5 = 0x09 */
#define SC2703_TORCH_TARGET_CH1_SHIFT                0
#define SC2703_TORCH_TARGET_CH1_MASK                 (0x1F << 0)
/* SC2703_FLASH_FD_CONFIG6 = 0x0a */
#define SC2703_FLASH_TARGET_CH1_SHIFT                0
#define SC2703_FLASH_TARGET_CH1_MASK                 (0x1F << 0)
/* SC2703_FLASH_FD_CONFIG7 = 0x0b */
#define SC2703_TORCH_TARGET_CH2_SHIFT                0
#define SC2703_TORCH_TARGET_CH2_MASK                 (0x1F << 0)
/* SC2703_FLASH_FD_CONFIG8 = 0x0c */
#define SC2703_FLASH_TARGET_CH2_SHIFT                0
#define SC2703_FLASH_TARGET_CH2_MASK                 (0x1F << 0)
/* SC2703_FLASH_FD_CONFIG9 = 0x0d */
#define SC2703_TUNE_CH1_SHIFT                        0
#define SC2703_TUNE_CH1_MASK                         (0x7 << 0)
#define SC2703_TUNE_CH2_SHIFT                        3
#define SC2703_TUNE_CH2_MASK                         (0x7 << 3)
/* SC2703_FLASH_FD_CONFIG10 = 0x0e */
#define SC2703_CP_CK_DELAY_SHIFT                     0
#define SC2703_CP_CK_DELAY_MASK                      (0x3 << 0)
#define SC2703_CP_DISCHG_DELAY_SHIFT                 2
#define SC2703_CP_DISCHG_DELAY_MASK                  (0x3 << 2)
#define SC2703_CP_BIAS_DELAY_SHIFT                   4
#define SC2703_CP_BIAS_DELAY_MASK                    (0x3 << 4)
#define SC2703_PREC_DELAY_SHIFT                      6
#define SC2703_PREC_DELAY_MASK                       (0x3 << 6)
/* SC2703_FLASH_FD_FAULT_CONFIG = 0x0f */
#define SC2703_E_CH1_OC_AUTO_SHUTDOWN_SHIFT          0
#define SC2703_E_CH1_OC_AUTO_SHUTDOWN_MASK           BIT(0)
#define SC2703_E_CH1_SC_AUTO_SHUTDOWN_SHIFT          1
#define SC2703_E_CH1_SC_AUTO_SHUTDOWN_MASK           BIT(1)
#define SC2703_E_CH2_OC_AUTO_SHUTDOWN_SHIFT          2
#define SC2703_E_CH2_OC_AUTO_SHUTDOWN_MASK           BIT(2)
#define SC2703_E_CH2_SC_AUTO_SHUTDOWN_SHIFT          3
#define SC2703_E_CH2_SC_AUTO_SHUTDOWN_MASK           BIT(3)
#define SC2703_E_OT_AUTO_SHUTDOWN_SHIFT              4
#define SC2703_E_OT_AUTO_SHUTDOWN_MASK               BIT(4)
#define SC2703_E_AUTO_REDUCE_TORCH_CURRENT_SHIFT     5
#define SC2703_E_AUTO_REDUCE_TORCH_CURRENT_MASK      BIT(5)
#define SC2703_TORCH_TARGET_REDUCE_SHIFT             6
#define SC2703_TORCH_TARGET_REDUCE_MASK              (0x3 << 6)

#endif
