/*
 * sound/soc/sprd/tdm-null-codec.c
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
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt("TDMNC")""fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <sound/soc.h>
#include "sprd-asoc-common.h"
#include "sprd-tdm.h"

#include "sprd-asoc-card-utils.h"

#define NAME_SIZE	32

static void tdm_register_proc_init(struct snd_soc_card *card)
{
}

static int tdm_debug_init(struct snd_soc_card *card)
{
	return 0;
}


static int board_late_probe(struct snd_soc_card *card)
{
	tdm_debug_init(card);
	tdm_register_proc_init(card);
	return 0;
}

#define TDM_CONFIG(xname, xreg) \
	SOC_SINGLE_EXT(xname, FUN_REG(xreg), 0, INT_MAX, 0, \
		tdm_config_get, tdm_config_set)

static const struct snd_kcontrol_new tdm_config_snd_controls[] = {
	TDM_CONFIG("tdm_fs", TDM_FS),
	TDM_CONFIG("trx_mst_mode", MST_MODE),
	TDM_CONFIG("duplex_mode", DUPLEX_MODE),
	TDM_CONFIG("trx_threshold", TRX_THRESHOLD),
	TDM_CONFIG("trx_data_width", TRX_DATA_WIDTH),
	TDM_CONFIG("trx_data_mode", TRX_DATA_MODE),
	TDM_CONFIG("trx_slot_width", TRX_SLOT_WIDTH),
	TDM_CONFIG("trx_slot_num", TRX_SLOT_NUM),
	TDM_CONFIG("trx_msb_mode", TRX_MSB_MODE),
	TDM_CONFIG("trx_pulse_mode", TRX_PULSE_MODE),
	TDM_CONFIG("tdm_slot_valid", TDM_SLOT_VALID),
	TDM_CONFIG("trx_sync_mode", TRX_SYNC_MODE),
	TDM_CONFIG("tdm_lrck_inv", TDM_LRCK_INV),
	TDM_CONFIG("tx_bck_pos_dly", TX_BCK_POS_DLY),
	TDM_CONFIG("rx_bck_pos_dly", RX_BCK_POS_DLY),
};

struct sprd_array_size sprd_alltdm_card_controls = {
	.ptr = tdm_config_snd_controls,
	.size = ARRAY_SIZE(tdm_config_snd_controls),
};

static int sprd_asoc_tdm_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_soc_card *card;

	ret = asoc_sprd_card_probe(pdev, &card);
	if (ret) {
		pr_err("ERR: %s, asoc_sprd_card_probe failed!\n", __func__);
		return ret;
	}

	/* Add your special configurations here */
	/* Add card special kcontrols */
	card->controls = sprd_alltdm_card_controls.ptr;
	card->num_controls = sprd_alltdm_card_controls.size;


	card->late_probe = board_late_probe;

	return asoc_sprd_register_card(&pdev->dev, card);
}

#ifdef CONFIG_OF
static const struct of_device_id tdm_null_codec_of_match[] = {
	{.compatible = "unisoc,i2s-null-codec",},
	{},
};

MODULE_DEVICE_TABLE(of, tdm_null_codec_of_match);
#endif

static struct platform_driver sprd_asoc_tdm_driver = {
	.driver = {
		.name = "i2s-null-codec",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tdm_null_codec_of_match,
	},
	.probe = sprd_asoc_tdm_probe,
	.remove = asoc_sprd_card_remove,
};

static int __init sprd_asoc_tdm_init(void)
{
	return platform_driver_register(&sprd_asoc_tdm_driver);
}

late_initcall_sync(sprd_asoc_tdm_init);

MODULE_DESCRIPTION("ALSA SoC SpreadTrum TDM");
MODULE_LICENSE("GPL");
MODULE_ALIAS("machine:tdm");
