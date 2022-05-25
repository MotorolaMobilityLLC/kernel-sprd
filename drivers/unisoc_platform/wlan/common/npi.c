/*
* SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: GPL-2.0
*
* Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of version 2 of the GNU General Public License
* as published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <misc/marlin_platform.h>
#include <net/genetlink.h>

#include "chip_ops.h"
#include "debug.h"
#include "iface.h"
#include "npi.h"

static int npi_nl_send_generic(struct genl_info *info, u8 attr, u8 cmd, u32 len,
			       u8 *data);

static int npi_pre_doit(const struct genl_ops *ops,
			struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *ndev;
	struct sprd_vif *vif;
	struct sprd_priv *priv;
	int ifindex;

	if (!info) {
		pr_err("%s NULL info!\n", __func__);
		return -EINVAL;
	}

	if (info->attrs[SPRD_NL_ATTR_IFINDEX]) {
		ifindex = nla_get_u32(info->attrs[SPRD_NL_ATTR_IFINDEX]);
		ndev = dev_get_by_index(genl_info_net(info), ifindex);
		if (!(ndev && (ndev->flags & IFF_UP))) {
			pr_err("%s NPI: net device is not ready yet\n", __func__);
			return -EFAULT;
		}
		vif = netdev_priv(ndev);
		priv = vif->priv;
		info->user_ptr[0] = ndev;
		info->user_ptr[1] = priv;
	} else {
		pr_err("nl80211_pre_doit: Not have attr_ifindex\n");
		return -EFAULT;
	}
	return 0;
}

static void npi_post_doit(const struct genl_ops *ops,
			  struct sk_buff *skb, struct genl_info *info)
{
	if (info->user_ptr[0])
		dev_put(info->user_ptr[0]);
}

static int npi_nl_handler(struct sk_buff *skb_2, struct genl_info *info)
{
	struct net_device *ndev = NULL;
	struct sprd_vif *vif = NULL;
	struct sprd_priv *priv = NULL;
	struct sprd_npi_cmd_hdr *hdr = NULL;
	unsigned short r_len = 1024, s_len;
	unsigned char *s_buf = NULL, *r_buf = NULL;
	unsigned char dbgstr[64] = { 0 };
	int err = -100, ret = 0;
	const char *id_name = NULL;
	unsigned char status = 0;
	const char *vendor = "UniSoC,";

	ndev = info->user_ptr[0];
	vif = netdev_priv(ndev);
	priv = info->user_ptr[1];
	if (!info->attrs[SPRD_NL_ATTR_AP2CP]) {
		pr_err("%s: invalid content\n", __func__);
		return -EPERM;
	}
	r_buf = kmalloc(1024, GFP_KERNEL);
	if (!r_buf)
		return -ENOMEM;

	s_buf = nla_data(info->attrs[SPRD_NL_ATTR_AP2CP]);
	s_len = nla_len(info->attrs[SPRD_NL_ATTR_AP2CP]);

	sprintf(dbgstr, "[iwnpi][SEND][%d]:", s_len);
	hdr = (struct sprd_npi_cmd_hdr *)s_buf;
	pr_err("%s type is %d, subtype %d\n", dbgstr, hdr->type, hdr->subtype);

	if (hdr->subtype == SPRD_NPI_CMD_SET_COUNTRY) {
		char *country = s_buf + sizeof(struct sprd_npi_cmd_hdr);
		/*no need send npi command to firmware*/
		pr_err("%s show country code : %c%c\n", __func__, country[0], country[1]);
		err = regulatory_hint(priv->wiphy, country);
		hdr->len = sizeof(int);
		hdr->type = SPRD_CP2HT_REPLY;
		r_len = sizeof(*hdr) + hdr->len;
		memcpy(r_buf, hdr, sizeof(*hdr));
		memcpy(r_buf + sizeof(*hdr), &err, hdr->len);
	} else if (hdr->subtype == SPRD_NPI_CMD_GET_CHIPID) {
		id_name = (char *)wcn_get_chip_name();
		sprintf(r_buf, "%d", status);
		strcat(r_buf, vendor);
		strcat(r_buf, id_name);
		r_len = strlen(r_buf);
		pr_err("r_len = %d, %s\n", r_len, __func__);
	} else if (hdr->subtype == SPRD_NPI_CMD_SET_RANDOM_MAC) {
		char *rand_mac = s_buf + sizeof(struct sprd_npi_cmd_hdr);
		priv->rand_mac_flag = *((unsigned int *)rand_mac);
		pr_err("%s NPI random mac flag%d\n", dbgstr, priv->rand_mac_flag);
		hdr->len = sizeof(int);
		hdr->type = SPRD_CP2HT_REPLY;
		r_len = sizeof(*hdr) + hdr->len;
		memcpy(r_buf, hdr, sizeof(*hdr));
		memcpy(r_buf + sizeof(*hdr), &ret, hdr->len);
	} else {
		sprd_npi_send_recv(priv, vif, s_buf, s_len, r_buf, &r_len);

		sprintf(dbgstr, "[iwnpi][RECV][%d]:", r_len);
		hdr = (struct sprd_npi_cmd_hdr *)r_buf;
		pr_err("%s type is %d, subtype %d\n", dbgstr, hdr->type,
		       hdr->subtype);
	}

	ret = npi_nl_send_generic(info, SPRD_NL_ATTR_CP2AP,
				  SPRD_NL_CMD_NPI, r_len, r_buf);

	kfree(r_buf);
	return ret;
}

static int npi_nl_get_info_handler(struct sk_buff *skb_2,
				   struct genl_info *info)
{
	struct net_device *ndev = info->user_ptr[0];
	struct sprd_vif *vif = netdev_priv(ndev);
	unsigned char r_buf[64] = { 0 };
	unsigned short r_len = 0;
	int ret = 0;

	if (vif) {
		ether_addr_copy(r_buf, vif->ndev->dev_addr);
		sprd_put_vif(vif);
		r_len = 6;
		ret = npi_nl_send_generic(info, SPRD_NL_ATTR_CP2AP,
				      SPRD_NL_CMD_GET_INFO, r_len, r_buf);
	} else {
		pr_err("%s NULL vif!\n", __func__);
		ret = -1;
	}
	return ret;
}

static struct nla_policy sprd_genl_policy[SPRD_NL_ATTR_MAX + 1] = {
	[SPRD_NL_ATTR_IFINDEX] = {.type = NLA_U32},
	[SPRD_NL_ATTR_AP2CP] = {.type = NLA_BINARY, .len = 1024},
	[SPRD_NL_ATTR_CP2AP] = {.type = NLA_BINARY, .len = 1024}
};

static struct genl_ops sprd_nl_ops[] = {
	{
		.cmd = SPRD_NL_CMD_NPI,
		.doit = npi_nl_handler,
	},
	{
		.cmd = SPRD_NL_CMD_GET_INFO,
		.doit = npi_nl_get_info_handler,
	}
};

static struct genl_family sprd_nl_genl_family = {
	.hdrsize = 0,
	.name = "SPRD_NL",
	.version = 1,
	.maxattr = SPRD_NL_ATTR_MAX,
	.policy = sprd_genl_policy,
	.pre_doit = npi_pre_doit,
	.post_doit = npi_post_doit,
	.module = THIS_MODULE,
	.ops = sprd_nl_ops,
	.n_ops = ARRAY_SIZE(sprd_nl_ops),
};

static int npi_nl_send_generic(struct genl_info *info, u8 attr, u8 cmd, u32 len,
			       u8 *data)
{
	struct sk_buff *skb;
	void *hdr;
	int ret;

	skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!skb)
		return -ENOMEM;
	hdr = genlmsg_put(skb, info->snd_portid, info->snd_seq,
			  &sprd_nl_genl_family, 0, cmd);
	if (IS_ERR(hdr)) {
		ret = PTR_ERR(hdr);
		goto err_put;
	}
	if (nla_put(skb, attr, len, data)) {
		ret = -1;
		goto err_put;
	}

	genlmsg_end(skb, hdr);
	return genlmsg_reply(skb, info);

err_put:
	nlmsg_free(skb);
	return ret;
}

void sprd_init_npi(void)
{
	int ret = genl_register_family(&sprd_nl_genl_family);

	if (ret)
		pr_err("genl_register_family error: %d\n", ret);
}

void sprd_deinit_npi(void)
{
	int ret = genl_unregister_family(&sprd_nl_genl_family);

	if (ret)
		pr_err("genl_unregister_family error:%d\n", ret);
}
