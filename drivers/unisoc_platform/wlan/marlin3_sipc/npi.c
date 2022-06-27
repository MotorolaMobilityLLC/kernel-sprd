/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Xianwei.Zhao <xianwei.zhao@spreadtrum.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <net/genetlink.h>
#include <linux/types.h>
#include <misc/marlin_platform.h>

#include "sprdwl.h"
#include "npi.h"

static int sprdwl_nl_send_generic(struct genl_info *info, u8 attr, u8 cmd,
				  u32 len, u8 *data);
static struct genl_family sprdwl_nl_genl_family;

static int sprdwl_npi_pre_doit(const struct genl_ops *ops,
			       struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *ndev;
	struct sprdwl_vif *vif;
	struct sprdwl_priv *priv;
	int ifindex;

	if (!info) {
		wl_err("%s NULL info!\n", __func__);
		return -EINVAL;
	}

	if (info->attrs[SPRDWL_NL_ATTR_IFINDEX]) {
		ifindex = nla_get_u32(info->attrs[SPRDWL_NL_ATTR_IFINDEX]);
		ndev = dev_get_by_index(genl_info_net(info), ifindex);
		if (!ndev) {
			wl_err("NPI: Could not find ndev\n");
			return -EFAULT;
		}
		vif = netdev_priv(ndev);
		priv = vif->priv;
		info->user_ptr[0] = ndev;
		info->user_ptr[1] = priv;
	} else {
		wl_err("nl80211_pre_doit: Not have attr_ifindex\n");
		return -EFAULT;
	}
	return 0;
}

static void sprdwl_npi_post_doit(const struct genl_ops *ops,
				 struct sk_buff *skb, struct genl_info *info)
{
	if (info->user_ptr[0])
		dev_put(info->user_ptr[0]);
}

static int sprdwl_nl_npi_handler(struct sk_buff *skb_2, struct genl_info *info)
{
	struct net_device *ndev = NULL;
	struct sprdwl_vif *vif = NULL;
	struct sprdwl_priv *priv = NULL;
	struct sprdwl_npi_cmd_hdr *hdr = NULL;
	unsigned short r_len = 1024, s_len;
	unsigned char *s_buf = NULL, *r_buf = NULL, *rand_mac = NULL;
	unsigned char dbgstr[64] = { 0 };
	int ret = 0;

	ndev = info->user_ptr[0];
	vif = netdev_priv(ndev);
	priv = info->user_ptr[1];
	if (!info->attrs[SPRDWL_NL_ATTR_AP2CP]) {
		wl_err("%s: invalid content\n", __func__);
		return -EPERM;
	}
	r_buf = kmalloc(1024, GFP_KERNEL);
	if (!r_buf)
		return -ENOMEM;

	s_buf = nla_data(info->attrs[SPRDWL_NL_ATTR_AP2CP]);
	s_len = nla_len(info->attrs[SPRDWL_NL_ATTR_AP2CP]);

	sprintf(dbgstr, "[iwnpi][SEND][%d]:", s_len);
	hdr = (struct sprdwl_npi_cmd_hdr *)s_buf;
	wl_err("%s type is %d, subtype %d\n", dbgstr, hdr->type, hdr->subtype);
	if (hdr->subtype == SPRDWL_NPI_CMD_SET_RANDOM_MAC) {
		rand_mac = s_buf + sizeof(struct sprdwl_npi_cmd_hdr);
		priv->rand_mac_flag = *((unsigned int *)rand_mac);
		pr_err("%s NPI random mac flag%d\n", dbgstr, priv->rand_mac_flag);
		hdr->len = sizeof(int);
		hdr->type = SPRDWL_CP2HT_REPLY;
		r_len = sizeof(*hdr) + hdr->len;
		memcpy(r_buf, hdr, sizeof(*hdr));
		memcpy(r_buf + sizeof(*hdr), &ret, hdr->len);
	} else {
		sprdwl_npi_send_recv(priv, vif->ctx_id, s_buf, s_len, r_buf, &r_len);
		sprintf(dbgstr, "[iwnpi][RECV][%d]:", r_len);
		hdr = (struct sprdwl_npi_cmd_hdr *)r_buf;
		wl_err("%s type is %d, subtype %d\n", dbgstr, hdr->type, hdr->subtype);
	}
	ret = sprdwl_nl_send_generic(info, SPRDWL_NL_ATTR_CP2AP,
				     SPRDWL_NL_CMD_NPI, r_len, r_buf);

	kfree(r_buf);
	return ret;
}


static int sprdwl_nl_get_info_handler(struct sk_buff *skb_2,
				      struct genl_info *info)
{
	struct net_device *ndev = info->user_ptr[0];
	struct sprdwl_vif *vif = netdev_priv(ndev);
	unsigned char r_buf[64] = { 0 };
	unsigned short r_len = 0;
	int ret = 0;

	wl_err("%s enter\n", __func__);

	if (vif) {
		ether_addr_copy(r_buf, vif->ndev->dev_addr);
		sprdwl_put_vif(vif);
		r_len = 6;
		ret = sprdwl_nl_send_generic(info, SPRDWL_NL_ATTR_CP2AP,
					     SPRDWL_NL_CMD_GET_INFO, r_len,
					     r_buf);
	} else {
		wl_err("%s NULL vif!\n", __func__);
		ret = -1;
	}
	return ret;
}

static struct nla_policy sprdwl_genl_policy[SPRDWL_NL_ATTR_MAX + 1] = {
	[SPRDWL_NL_ATTR_IFINDEX] = {.type = NLA_U32},
	[SPRDWL_NL_ATTR_AP2CP] = {.type = NLA_BINARY, .len = 1024},
	[SPRDWL_NL_ATTR_CP2AP] = {.type = NLA_BINARY, .len = 1024}
};

static struct genl_ops sprdwl_nl_ops[] = {
	{
		.cmd = SPRDWL_NL_CMD_NPI,
		.doit = sprdwl_nl_npi_handler,
	},
	{
		.cmd = SPRDWL_NL_CMD_GET_INFO,
		.doit = sprdwl_nl_get_info_handler,
	}
};

static struct genl_family sprdwl_nl_genl_family = {
	.hdrsize = 0,
	.name = "SPRD_NL",
	.version = 1,
	.maxattr = SPRDWL_NL_ATTR_MAX,
	.policy = sprdwl_genl_policy,
	.pre_doit = sprdwl_npi_pre_doit,
	.post_doit = sprdwl_npi_post_doit,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	.module = THIS_MODULE,
	.n_ops = ARRAY_SIZE(sprdwl_nl_ops),
	.ops = sprdwl_nl_ops,
#endif
};

static int sprdwl_nl_send_generic(struct genl_info *info, u8 attr,
				  u8 cmd, u32 len, u8 *data)
{
	struct sk_buff *skb;
	void *hdr;
	int ret;

	skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	if (!skb)
		return -ENOMEM;
	hdr = genlmsg_put(skb, info->snd_portid, info->snd_seq,
			  &sprdwl_nl_genl_family, 0, cmd);
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

void sprdwl_init_npi(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	int ret = genl_register_family(&sprdwl_nl_genl_family);
	if (ret)
		wl_err("genl_register_family error: %d\n", ret);
#else
	int ret = genl_register_family_with_ops(&sprdwl_nl_genl_family,
						sprdwl_nl_ops);
	if (ret)
		wl_err("genl_register_family_with_ops error: %d\n", ret);
#endif
}

void sprdwl_deinit_npi(void)
{
	int ret = genl_unregister_family(&sprdwl_nl_genl_family);

	if (ret)
		wl_err("genl_unregister_family error:%d\n", ret);
}
