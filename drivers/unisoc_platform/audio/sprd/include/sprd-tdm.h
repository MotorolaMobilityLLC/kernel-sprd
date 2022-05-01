/*
 * sound/soc/sprd/sprd-tdm.h
 *
 * Copyright (C) 2015 SpreadTrum Ltd.
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

#define TDM_MAGIC_ID	(0x129)
#define FUN_REG(f) ((unsigned short)(-((f) + 1)))
#define TDM_FIFO_DEPTH 32

enum {
    TDM_FS = 0,
    MST_MODE,
    DUPLEX_MODE,
    TRX_THRESHOLD,
    TRX_DATA_WIDTH,
    TRX_DATA_MODE,
    TRX_SLOT_WIDTH,
    TRX_SLOT_NUM,
    TRX_MSB_MODE,
    TRX_PULSE_MODE,
    TDM_SLOT_VALID,
    TRX_SYNC_MODE,
    TDM_LRCK_INV,
    TX_BCK_POS_DLY,
    RX_BCK_POS_DLY,
};

struct tdm_config {
    u32 tdm_fs;
    u32 trx_mst_mode;
    u32 duplex_mode;
    u32 trx_threshold;
    u32 trx_data_width;
    u32 trx_data_mode;
    u32 trx_slot_width;
    u32 trx_slot_num;
    u32 trx_msb_mode;
    u32 trx_pulse_mode;
    u32 tdm_slot_valid;
    u32 trx_sync_mode;
    u32 slave_timeout;
    u32 tdm_lrck_inv;
    u32 byte_per_chan:2;
    u16 tx_watermark;
    u16 rx_watermark;
    u32 tx_bck_pos_dly;
    u32 rx_bck_pos_dly;
};

#if (defined(CONFIG_SND_SOC_UNISOC_TDM) || defined(CONFIG_SND_SOC_UNISOC_TDM_MODULE))
int tdm_config_get(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol);
int tdm_config_set(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol);

struct tdm_config *sprd_tdm_dai_to_config(struct snd_soc_dai *dai);

#else
static inline int tdm_config_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s is empty.\n", __func__);
	return 0;
}

static inline int tdm_config_set(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s is empty.\n", __func__);
	return 0;
}

static inline struct tdm_config *sprd_tdm_dai_to_config(struct snd_soc_dai *dai)
{
	pr_debug("%s is empty.\n", __func__);
	return 0;
}
#endif /* CONFIG_SND_SOC_UNISOC_TDM */
