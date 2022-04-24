/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
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

enum {
    TDM_2SLOT = 0,
    TDM_4SLOT,
    TDM_8SLOT,
};

enum {
    TDM_16BIT = 0,
    TDM_24BIT,
    TDM_32BIT,
};

enum {
    TDM_I2S_COMPATIBLE = 0,
    TDM_LEFT_JUSTIFILED,
    TDM_RIGHT_JUSTIFILED,
};

enum {
    USE_LRCK_P_CAPTURE = 0,
    USE_LRCK_N_CAPTURE,
};

enum {
    TDM_LSB = 0,
    TDM_MSB,
};

enum {
    TDM_LRCK = 0,
    TDM_BCK_PLUSE,
    TDM_SLOT_PLUSE,
};

enum {
    TDM_SLAVE = 0,
    TDM_MASTER,
};

enum {
    TDM_IDLE = 0,
    TDM_WAIT,
    TDM_SYNC,
    TDM_TRAN,
};

enum {
    TDM_FULL_DUPLEX = 0,
    TDM_HALF_DUPLEX_RX = 1,
    TDM_HALF_DUPLEX_TX = 3,
};

enum {
    TDM_MSBJUSTFIED = 0,
    TDM_COMPATIBLE,
};

enum {
    LRCK_INVERT_I2S = 0,
    LRCK_INVERT_RJ_LJ,
};

enum {
    SET_TX_REG = 0,
    SET_RX_REG,
};

#define GLB_MODULE_EN0_STS 0x0
#define TDM_HF_EN BIT(27)
#define TDM_EN BIT(28)

#define GLB_MODULE_EN1_STS 0x04
#define DMA_TDM_RX_SEL(x) (((x) & 0x3) << 24)
#define DMA_TDM_TX_SEL(x) (((x) & 0x3) << 22)

#define GLB_MODULE_RST0_STS 0x08
#define TDM_HF_SOFT_RST BIT(29)
#define TDM_SOFT_RST BIT(27)

/* aud_cp_clk_rf */
#define AUDCP_CLK_RF_CGM_TDM_SLV_SEL 0xb8
#define CGM_TDM_SLV_PAD_SEL BIT(16)

#define AUDCP_CLK_RF_CGM_TDM_SEL 0xe8
#define CGM_TDM_SEL BIT(0)

#define AUDCP_CLK_RF_CGM_TDM_HF_SEL 0xf4
#define CGM_TDM_HF_SEL_CLK BIT(0)

/* aud_cp_dvfs_apb_rf */
#define AUDCP_DVFS_FREQ_UPDATE_BYPASS0 0x0048
#define REG_TDM_HF_FREQ_UPD_EN_BYP BIT(5)

#define TDM_HF_INDEX0_MAP 0x01f4
#define TDM_HF_VOL_INDEX0(x) (((x) & 0xf) << 1)

#define AUD_CP_DFS_IDLE_DISABLE 0x09b4
#define TDM_HF_DFS_IDLE_DISABLE BIT(5)

#define AD_CP_DVFS_CLK_REG_CFG0 0x0a58
#define CGM_TDM_HF_SEL BIT(30)
#define CGM_DSP_CORE_SEL(x) ((x) & 0x7)

/* aon_apb_rf */
#define AUDCP_CTRL 0x014c
#define AON_2_AUD_ACCESS_EN BIT(0)
#define AP_2_AUD_ACCESS_EN BIT(5)

/* register offset */
#define TDM_REG_TX_CTRL		(0x0000)
#define TDM_REG_TX_CFG0		(0x0004)
#define TDM_REG_TX_CFG1		(0x0008)
#define TDM_REG_TX_STAT		(0x000C)
#define TDM_REG_TX_CFG2		(0x0010)
#define TDM_REG_RX_CTRL		(0x0020)
#define TDM_REG_RX_CFG0		(0x0024)
#define TDM_REG_RX_CFG1		(0x0028)
#define TDM_REG_RX_STAT		(0x002C)
#define TDM_REG_RX_CFG2		(0x0030)
#define TDM_REG_MISC_CTRL	(0x0040)
#define TDM_REG_INT_STAT	(0x0044)
#define TDM_REG_TX_FIFO     (0x0400)
#define TDM_REG_RX_FIFO     (0x0400)

/*
 * Register Name   : TDM_REG_TX_CTRL
 * Register Offset : 0x0000
 * Description     : TDM TX control register
 */

#define TDM_TX_CTRL_BCK_POS_DLY(x)   (((x) & 0x7) << 4)
#define TDM_TX_CTRL_IO_EN            BIT(3)
#define TDM_TX_CTRL_TIMEOUT_EN       BIT(2)
#define TDM_TX_CTRL_SOFT_RST         BIT(1)
#define TDM_TX_CTRL_ENABLE           BIT(0)

/*
 * Register Name   : TDM_REG_TX_CFG0
 * Register Offset : 0x0004
 * Description     : TDM TX configuration 0 register
 */

#define TDM_TX_CFG0_THRESHOLD(x)     (((x) & 0xFF) << 24)
#define TDM_TX_CFG0_SLOT_VALID(x)    (((x) & 0xFF) << 16)
#define TDM_TX_CFG0_DATA_WIDTH(x)    (((x) & 0x3) << 14)
#define TDM_TX_CFG0_DATA_MODE(x)     (((x) & 0x3) << 12)
#define TDM_TX_CFG0_SLOT_WIDTH(x)    (((x) & 0x3) << 10)
#define TDM_TX_CFG0_SLOT_NUM(x)      (((x) & 0x3) << 8)
#define TDM_TX_CFG0_SYNC_EN          BIT(6)
#define TDM_TX_CFG0_MSB_MODE         BIT(5)
#define TDM_TX_CFG0_SYNC_MODE        BIT(4)
#define TDM_TX_CFG0_LRCK_INVERT      BIT(3)
#define TDM_TX_CFG0_PULSE_MODE(x)    (((x) & 0x3) << 1)
#define TDM_TX_CFG0_MST_MODE         BIT(0)

/*
 * Register Name   : TDM_REG_TX_CFG1
 * Register Offset : 0x0008
 * Description     : TDM TX configuration 1 register
 */

#define TDM_TX_CFG1_LRCK_DIV_NUM(x)     (((x) & 0xFFFF) << 16)
#define TDM_TX_CFG1_BCK_DIV_NUM(x)      (((x) & 0xFFFF))

/*
 * Register Name   : TDM_REG_TX_STAT
 * Register Offset : 0x000c
 * Description     : TDM TX status register
 */

#define TDM_TX_STAT_TXFIFO_EMPTY        BIT(3)
#define TDM_TX_STAT_TXFIFO_FULL         BIT(2)
#define TDM_TX_STAT_TX_ST(x)            (((x) & 0x3))

/*
 * Register Name   : TDM_REG_TX_CFG2
 * Register Offset : 0x0010
 * Description     : TDM TX configuration 2 register
 */

#define TDM_TX_CFG2_COUNT_VALID         BIT(31)
#define TDM_TX_CFG2_COUNT_VALUE(x)      (((x) & 0x7FFFFFFF))

/*
 * Register Name   : TDM_REG_RX_CTRL
 * Register Offset : 0x0020
 * Description     : TDM RX control register
 */

#define TDM_RX_CTRL_BCK_POS_DLY(x)   (((x) & 0x7) << 4)
#define TDM_RX_CTRL_IO_EN            BIT(3)
#define TDM_RX_CTRL_TIMEOUT_EN       BIT(2)
#define TDM_RX_CTRL_SOFT_RST         BIT(1)
#define TDM_RX_CTRL_ENABLE           BIT(0)

/*
 * Register Name   : TDM_REG_RX_CFG0
 * Register Offset : 0x0024
 * Description     : TDM RX configuration 0 register
 */

#define TDM_RX_CFG0_THRESHOLD(x)     (((x) & 0xFF) << 24)
#define TDM_RX_CFG0_SLOT_VALID(x)    (((x) & 0xFF) << 16)
#define TDM_RX_CFG0_DATA_WIDTH(x)    (((x) & 0x3) << 14)
#define TDM_RX_CFG0_DATA_MODE(x)     (((x) & 0x3) << 12)
#define TDM_RX_CFG0_SLOT_WIDTH(x)    (((x) & 0x3) << 10)
#define TDM_RX_CFG0_SLOT_NUM(x)      (((x) & 0x3) << 8)
#define TDM_RX_CFG0_SYNC_EN          BIT(6)
#define TDM_RX_CFG0_MSB_MODE         BIT(5)
#define TDM_RX_CFG0_SYNC_MODE        BIT(4)
#define TDM_RX_CFG0_LRCK_INVERT      BIT(3)
#define TDM_RX_CFG0_PULSE_MODE(x)    (((x) & 0x3) << 1)
#define TDM_RX_CFG0_MST_MODE         BIT(0)

/*
 * Register Name   : TDM_REG_RX_CFG1
 * Register Offset : 0x0028
 * Description     : TDM RX configuration 1 register
 */

#define TDM_RX_CFG1_LRCK_DIV_NUM(x)     (((x) & 0xFFFF) << 16)
#define TDM_RX_CFG1_BCK_DIV_NUM(x)      (((x) & 0xFFFF))

/*
 * Register Name   : TDM_REG_RX_STAT
 * Register Offset : 0x002c
 * Description     : TDM TX status register
 */

#define TDM_RX_STAT_RXFIFO_EMPTY        BIT(3)
#define TDM_RX_STAT_RXFIFO_FULL         BIT(2)
#define TDM_RX_STAT_RX_ST(x)            (((x) & 0x3))

/*
 * Register Name   : TDM_REG_RX_CFG2
 * Register Offset : 0x0030
 * Description     : TDM TX configuration 2 register
 */

#define TDM_RX_CFG2_COUNT_VALID         BIT(31)
#define TDM_RX_CFG2_COUNT_VALUE(x)      (((x) & 0x7FFFFFFF))

/*
 * Register Name   : TDM_REG_MISC_CTRL
 * Register Offset : 0x0040
 * Description     : TDM misc control register
 */

#define TDM_MISC_CTRL_TIMEOUT_THR(x)    (((x) & 0xFFFF) << 16)
#define TDM_MISC_CTRL_HALF_DUPLEX_MODE  BIT(4)
#define TDM_MISC_CTRL_HALF_DUPLEX_EN    BIT(3)
#define TDM_MISC_CTRL_AUTO_GATE_EN      BIT(2)
#define TDM_MISC_CTRL_DMA_EN            BIT(1)
#define TDM_MISC_CTRL_INT_EN            BIT(0)

/*
 * Register Name   : TDM_REG_INT_STAT
 * Register Offset : 0x0044
 * Description     : TDM interrupt status register
 */

#define TDM_INT_STAT_RX_TIMEOUT             BIT(5)
#define TDM_INT_STAT_TX_TIMEOUT             BIT(4)
#define TDM_INT_STAT_RXFIFO_OVERFLOW        BIT(3)
#define TDM_INT_STAT_TXFIFO_UNDERFLOW       BIT(2)
#define TDM_INT_STAT_RXFIFO_ALMOST_FULL     BIT(1)
#define TDM_INT_STAT_TXFIFO_ALMOST_EMPTY    BIT(0)

/*
 * Register Name   : TDM_REG_TX_FIFO
 * Register Offset : 0x0400
 * Description     : TDM TX FIFO write data register
 */

#define TDM_TX_FIFO_DATA(x)     (((x) & 0xFFFFFFFF))

/*
 * Register Name   : TDM_REG_RX_FIFO
 * Register Offset : 0x0400
 * Description     : TDM TX FIFO read data register
 */

#define TDM_RX_FIFO_DATA(x)     (((x) & 0xFFFFFFFF))

#define SAMPLATE_MIN 48000
#define SAMPLATE_MAX 96000
