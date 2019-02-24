/*
 * Copyright (C) 2018-2019 Unisoc Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#define pr_fmt(fmt) "sipa_dele: %s " fmt, __func__

#include <linux/sipa.h>
#include <linux/sipc.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "sipa_dele_priv.h"
#include "../pam_ipa/pam_ipa_core.h"

static struct ap_delegator *s_ap_delegator;

void ap_dele_on_open(void *priv, u16 flag, u32 data)
{
	int ret;
	struct smsg msg;
	struct sipa_to_pam_info ep_info;
	struct sipa_delegator *delegator = priv;

	/* call base class on_open func first */
	sipa_dele_on_open(priv, flag, data);

	/* do ap_delegator operations */
	sipa_get_ep_info(SIPA_EP_PCIE, &ep_info);

	/* dl tx */
	smsg_set(&msg, delegator->chan,
		 SMSG_TYPE_EVENT,
		 SMSG_FLG_DELE_ADDR_DL_TX,
		 ep_info.dl_fifo.tx_fifo_base_addr);
	ret = smsg_send(delegator->dst, &msg, -1);
	if (ret)
		pr_err("dl tx smsg send fail %d\n", ret);

	/* dl rx */
	smsg_set(&msg, delegator->chan,
		 SMSG_TYPE_EVENT,
		 SMSG_FLG_DELE_ADDR_DL_RX,
		 ep_info.dl_fifo.rx_fifo_base_addr);
	ret = smsg_send(delegator->dst, &msg, -1);
	if (ret)
		pr_err("dl rx smsg send fail %d\n", ret);
	/* ul tx */
	smsg_set(&msg, delegator->chan,
		 SMSG_TYPE_EVENT,
		 SMSG_FLG_DELE_ADDR_UL_TX,
		 ep_info.ul_fifo.tx_fifo_base_addr);
	ret = smsg_send(delegator->dst, &msg, -1);
	if (ret)
		pr_err("ul tx smsg send fail %d\n", ret);
	/* ul rx */
	smsg_set(&msg, delegator->chan,
		 SMSG_TYPE_EVENT,
		 SMSG_FLG_DELE_ADDR_UL_RX,
		 ep_info.ul_fifo.rx_fifo_base_addr);
	ret = smsg_send(delegator->dst, &msg, -1);
	if (ret)
		pr_err("ul rx smsg send fail %d\n", ret);
}

int ap_delegator_init(struct sipa_delegator_create_params *params)
{
	int ret;

	s_ap_delegator = devm_kzalloc(params->pdev,
				      sizeof(*s_ap_delegator),
				      GFP_KERNEL);
	if (!s_ap_delegator)
		return -ENOMEM;
	ret = sipa_delegator_init(&s_ap_delegator->delegator,
				  params);
	if (ret)
		return ret;

	s_ap_delegator->delegator.on_open = ap_dele_on_open;

	sipa_delegator_start(&s_ap_delegator->delegator);

	return 0;
}
